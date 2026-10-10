#pragma once

#include "field_glsl.h"

#include <QByteArray>
#include <QString>

#include <map>
#include <optional>
#include <string>

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

// The suffixes File > Open and drag and drop accept, lower case.
bool isGraphFileName(const QString& path);
