#include "graph_scene.h"

#include <cctype>

namespace fg = dce::fieldgraph;

fg::GlslScene compileGraphText(const std::string& text, bool jsonHint,
                               fg::MeshResolver& meshes) {
  std::size_t i = 0;
  while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
  const bool isJson = jsonHint || (i < text.size() && text[i] == '{');
  const fg::GraphNode root = isJson ? fg::parseJson(text) : fg::parseShorthand(text);

  // The exporter path builds the field once: it validates the graph and gives
  // the bounds the codegen traces in.
  dualc::BBox bounds = fg::FieldGraph::build(root, meshes).field().bounds();
  if (!bounds.isValid())
    throw fg::GraphError("the field has no finite bounds (infinite source or repeat)",
                         root.pointer);
  const dualc::Vector3 pad = bounds.extent() * 0.05;
  bounds = dualc::BBox{bounds.min - pad, bounds.max + pad};

  return fg::compileToGlsl(root, meshes, bounds);
}
