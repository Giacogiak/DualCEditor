#pragma once

#include "field_glsl.h"
#include "field_graph.h"

#include <QByteArray>
#include <QString>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>

// DualC's FileMeshResolver with relative paths tried against `dir` first, so
// a graph finds the meshes that sit beside it. An empty `dir` resolves against
// the working directory only. An `id=` mesh comes from the process's mesh
// library (mesh_library.h), registered with DualC's InMemoryMeshResolver on
// first use.
class GraphDirMeshResolver : public dce::fieldgraph::MeshResolver {
 public:
  explicit GraphDirMeshResolver(QString dir) : dir_(std::move(dir)) {}
  dce::fieldgraph::MeshHandle resolve(const std::string& path) override;
  dce::fieldgraph::MeshHandle resolveId(const std::string& id) override;

 private:
  QString dir_;
  dce::fieldgraph::FileMeshResolver files_;
  dce::fieldgraph::InMemoryMeshResolver ids_;
  std::set<std::string> registered_;
};

// DualC's GLSL codegen (compileToGlsl, at the pinned commit) reads a `mesh` or
// `winding` source by its `path` only: an `id` is refused. This copy of
// `graph` names each id as the path kMeshIdPath + id instead, which
// GraphDirMeshResolver::resolve sends to resolveId. Only the codegen sees it;
// the scene's graphs keep the id (docs/design/07-mesh-import.md).
inline constexpr const char* kMeshIdPath = "dualceditor-mesh-id:";
dce::fieldgraph::GraphNode meshIdsAsPaths(dce::fieldgraph::GraphNode graph);

// The folder a graph read from `path` resolves its mesh paths against: the
// file's own folder, or none for a Qt resource.
QString graphMeshDir(const QString& path);

// One attempt to turn a graph file into a compiled scene. On failure `scene`
// is empty and `error` reads like the CLI's: "field-graph error at <pointer>:
// <message>", with `pointer` the JSON pointer DualC's GraphError carries
// (empty when the error names no node). `nodeOps` maps each node's pointer to
// its op, so a binding's nodePointer can be named ("gyroid", "onion").
struct GraphLoad {
  std::optional<dce::fieldgraph::GlslScene> scene;
  std::map<std::string, std::string> nodeOps;
  QString error;
  QString pointer;
};

// Compile graph text read from `path`. JSON when the name ends in .json,
// otherwise sniffed (a leading '{' is JSON, anything else the shorthand), as
// dualc_field_view does. A relative mesh `path=` resolves against the graph
// file's folder first, then against the working directory.
GraphLoad compileGraphFileText(const QByteArray& text, const QString& path);

// Read `path` (a file or a Qt resource) and compile it.
GraphLoad loadGraphFile(const QString& path);

// A graph file read and parsed, not compiled: what a scene object holds. On
// failure `graph` is empty and `error` reads as GraphLoad's does.
struct GraphParse {
  std::optional<dce::fieldgraph::GraphNode> graph;
  QString error;
};
GraphParse parseGraphFile(const QString& path);
// Parse graph text read from `path`, as compileGraphFileText reads it.
GraphParse parseGraphFileText(const QByteArray& text, const QString& path);

// The suffixes File > Open and drag and drop accept, lower case.
bool isGraphFileName(const QString& path);
