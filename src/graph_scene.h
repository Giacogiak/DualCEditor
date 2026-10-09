#pragma once

#include "field_glsl.h"
#include "field_graph.h"

#include <string>

// Graph text -> compiled GLSL scene, the way dualc_field_view does it: parse
// (JSON when `jsonHint` is set or the text starts with '{', else the
// shorthand), take the field's own bounds padded by 5% as the trace box, and
// compile with DualC's codegen. Throws dce::fieldgraph::GraphError, whose
// pointer() names the offending node; an unbounded field throws one too.
dce::fieldgraph::GlslScene compileGraphText(const std::string& text, bool jsonHint,
                                            dce::fieldgraph::MeshResolver& meshes);
