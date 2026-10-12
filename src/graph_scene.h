#pragma once

#include "field_glsl.h"
#include "field_graph.h"

#include <map>
#include <string>

// Graph text -> parsed graph: JSON when `jsonHint` is set or the text starts
// with '{', else DualC's shorthand. Throws dce::fieldgraph::GraphError.
dce::fieldgraph::GraphNode parseGraphText(const std::string& text, bool jsonHint);

// The trace box of `root`: the field's own bounds, built once through
// FieldGraph::build, padded by 5% on each side. Throws GraphError when the
// graph does not build or the field is unbounded.
dualc::BBox traceBounds(const dce::fieldgraph::GraphNode& root,
                        dce::fieldgraph::MeshResolver& meshes);

// Each node's op keyed by its pointer, into `ops`.
void collectNodeOps(const dce::fieldgraph::GraphNode& root,
                    std::map<std::string, std::string>& ops);

// Graph text -> compiled GLSL scene, the way dualc_field_view does it: parse
// (JSON when `jsonHint` is set or the text starts with '{', else the
// shorthand), take the field's own bounds padded by 5% as the trace box, and
// compile with DualC's codegen. Throws dce::fieldgraph::GraphError, whose
// pointer() names the offending node; an unbounded field throws one too.
// `nodeOps`, when given, receives each node's op keyed by its pointer, the key
// a UniformBinding's nodePointer names.
dce::fieldgraph::GlslScene compileGraphText(
    const std::string& text, bool jsonHint, dce::fieldgraph::MeshResolver& meshes,
    std::map<std::string, std::string>* nodeOps = nullptr);
