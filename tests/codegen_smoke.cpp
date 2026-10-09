// A field graph goes through DualC's parser and GLSL codegen and comes out as
// a trace shader that defines sceneSDF. Exits non-zero on any failure.

#include "field_glsl.h"
#include "field_graph.h"

#include <cstdio>
#include <exception>
#include <string>

namespace fg = dce::fieldgraph;

namespace {

int fail(const char* what) {
  std::fprintf(stderr, "codegen_smoke: %s\n", what);
  return 1;
}

}  // namespace

int main() {
  try {
    const fg::GraphNode root = fg::parseShorthand(
        "intersection(box(min=[-2,-2,-2],max=[2,2,2]),"
        "onion(normalize(gyroid(wavelength=1.0)),thickness=0.3))");
    fg::FileMeshResolver meshes;
    const dualc::BBox bounds{dualc::Vector3{-2.5, -2.5, -2.5},
                             dualc::Vector3{2.5, 2.5, 2.5}};
    const fg::GlslScene scene = fg::compileToGlsl(root, meshes, bounds);
    if (scene.nodeCount <= 0) return fail("no nodes compiled");

    const std::string frag = fg::assembleTraceShader(scene, /*emitES=*/false);
    if (frag.rfind("#version 330", 0) != 0) return fail("not a GLSL 330 shader");
    if (frag.find("sceneSDF") == std::string::npos) return fail("no sceneSDF");

    std::printf("codegen_smoke: %d nodes, %zu bindings, %zu bytes of GLSL\n",
                scene.nodeCount, scene.bindings.size(), frag.size());
    return 0;
  } catch (const std::exception& e) {
    return fail(e.what());
  }
}
