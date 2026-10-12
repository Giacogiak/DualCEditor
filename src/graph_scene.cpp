#include "graph_scene.h"

#include <cctype>

namespace fg = dce::fieldgraph;

namespace {

void collectOps(const fg::GraphNode& n, std::map<std::string, std::string>& ops) {
  ops[n.pointer] = n.op;
  for (const fg::GraphNode& c : n.in) collectOps(c, ops);
}

}  // namespace

fg::GraphNode parseGraphText(const std::string& text, bool jsonHint) {
  std::size_t i = 0;
  while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
  const bool isJson = jsonHint || (i < text.size() && text[i] == '{');
  return isJson ? fg::parseJson(text) : fg::parseShorthand(text);
}

dualc::BBox traceBounds(const fg::GraphNode& root, fg::MeshResolver& meshes) {
  // The exporter path builds the field once: it validates the graph and gives
  // the bounds the codegen traces in.
  dualc::BBox bounds = fg::FieldGraph::build(root, meshes).field().bounds();
  if (!bounds.isValid())
    throw fg::GraphError("the field has no finite bounds (infinite source or repeat)",
                         root.pointer);
  const dualc::Vector3 pad = bounds.extent() * 0.05;
  return dualc::BBox{bounds.min - pad, bounds.max + pad};
}

void collectNodeOps(const fg::GraphNode& root, std::map<std::string, std::string>& ops) {
  collectOps(root, ops);
}

fg::GlslScene compileGraphText(const std::string& text, bool jsonHint,
                               fg::MeshResolver& meshes,
                               std::map<std::string, std::string>* nodeOps) {
  const fg::GraphNode root = parseGraphText(text, jsonHint);
  if (nodeOps) collectOps(root, *nodeOps);
  return fg::compileToGlsl(root, meshes, traceBounds(root, meshes));
}
