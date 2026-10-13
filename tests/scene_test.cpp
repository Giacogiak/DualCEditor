// The scene lowering without GL: three objects, two of them the same graph,
// lower to one GlslScene whose functions and uniforms all carry their object's
// id, whose object uniforms hold the visible flag, the colour and DualC's
// localFromWorld matrix, whose trace box is the union of the moved boxes, and
// whose fragment shader takes the surface colour from sceneColor.

#include "graph_scene.h"
#include "scene.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <regex>
#include <set>
#include <string>

namespace fg = dce::fieldgraph;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "scene_test: FAIL %s\n", what);
    ++failures;
  }
}

bool near(double a, double b) { return std::abs(a - b) < 1e-5; }

}  // namespace

int main() {
  Scene scene;
  const fg::GraphNode sphere = parseGraphText("sphere(radius=0.5)", false);
  scene.add("a", sphere);
  scene.add("b", sphere).transform = dualc::Mat4::translation({2.0, 0.0, 0.0});
  scene.add("c", parseGraphText("gyroid(wavelength=0.5)", false));
  check(scene.objects().size() == 3, "three objects");
  check(scene.objects()[2].id == 3, "ids count from 1");
  check(scene.objects()[0].colour == kDualCBase, "the first object is DualC's grey");
  check(scene.objects()[1].colour != scene.objects()[2].colour &&
            scene.objects()[1].colour != kDualCBase,
        "the others take distinct palette colours");

  // An unbounded graph fails, naming its object.
  bool threw = false;
  try {
    lowerScene(scene);
  } catch (const fg::GraphError& e) {
    threw = std::string(e.what()).find("object 3 (c)") != std::string::npos;
  }
  check(threw, "an unbounded object fails and the error names it");
  scene.remove(3);
  scene.add("c", parseGraphText(
                     "intersection(box(min=[-1,-1,-1],max=[1,1,1]),gyroid(wavelength=0.5))",
                     false))
      .visible = false;
  check(scene.objects()[2].id == 4, "a removed id is not reused");

  const LoweredScene low = lowerScene(scene);
  const fg::GlslScene& s = low.scene;
  check(low.objects.size() == 3, "one lowered entry per object");

  // Every identifier the codegen made is namespaced, and no two objects share
  // a uniform even where their graphs are equal (DualC would dedup them).
  const std::string& src = s.generatedSource;
  check(src.find("uniform float u_selected;") != std::string::npos &&
            src.find("c = u_o2_colour; id = 2.0;") != std::string::npos &&
            src.find("abs(id - u_selected) < 0.5") != std::string::npos,
        "sceneColor knows the object of a hit and tints the selected one (u_selected)");
  check(!std::regex_search(src, std::regex(R"(\bf[0-9]+\b)")), "no bare fN left");
  check(!std::regex_search(src, std::regex(R"(\bu_n[0-9]+_)")), "no bare u_nN_ left");
  for (const char* fn : {"o1_sdf", "o2_sdf", "o4_sdf", "float sceneSDF(vec3 p)",
                         "vec3 sceneColor(vec3 p)"})
    check(src.find(fn) != std::string::npos, fn);
  std::set<std::string> names;
  for (const fg::UniformBinding& b : s.bindings) names.insert(b.name);
  check(names.size() == s.bindings.size(), "binding names are unique");

  for (const LoweredObject& o : low.objects) {
    const std::string prefix = "u_o" + std::to_string(o.id) + "_";
    for (int i = o.firstBinding; i < o.endBinding; ++i)
      check(s.bindings[std::size_t(i)].name.rfind(prefix, 0) == 0,
            "an object's bindings carry its id");
    check(o.transformBinding >= o.firstBinding && o.transformBinding < o.endBinding,
          "the transform binding is found");
    check(s.bindings[std::size_t(o.visibleBinding)].name == objectUniformName(o.id, "visible"),
          "visible uniform name");
    check(s.bindings[std::size_t(o.colourBinding)].name == objectUniformName(o.id, "colour"),
          "colour uniform name");
  }
  check(s.bindings[std::size_t(low.objects[0].visibleBinding)].value[0] == 1.0f,
        "a visible object uploads 1");
  check(s.bindings[std::size_t(low.objects[2].visibleBinding)].value[0] == 0.0f,
        "a hidden object uploads 0");
  const std::vector<float>& col = s.bindings[std::size_t(low.objects[1].colourBinding)].value;
  check(col.size() == 3 && col[0] == scene.objects()[1].colour[0], "colour value");
  // DualC uploads localFromWorld, row-major: translation +2 x inverts to -2.
  const std::vector<float>& m = s.bindings[std::size_t(low.objects[1].transformBinding)].value;
  check(m.size() == 16 && near(m[3], -2.0) && near(m[0], 1.0), "transform is localFromWorld");

  // The trace box is the union of the moved boxes, hidden objects included.
  check(near(low.objects[1].worldBounds.min.x, low.objects[1].localBounds.min.x + 2.0),
        "the world box is the moved local box");
  check(near(s.bounds.max.x, low.objects[1].worldBounds.max.x), "union max x");
  check(near(s.bounds.min.x, low.objects[2].worldBounds.min.x), "union includes hidden");
  check(s.stepScale < 1.0f, "a TPMS object makes the scene non-metric");
  check(near(s.featureScale, 0.5), "feature scale is the gyroid's wavelength");

  // The shader takes the colour from sceneColor; a single graph keeps DualC's.
  const std::string fs = assembleSceneShader(s);
  check(fs.find("vec3  base = sceneColor(hp);") != std::string::npos, "colour patched");
  check(fs.find("vec3(0.80, 0.82, 0.86);") == std::string::npos, "DualC's literal gone");
  const fg::GlslScene plain = fg::compileToGlsl(sphere, *std::make_unique<fg::FileMeshResolver>(),
                                                dualc::BBox{{-1, -1, -1}, {1, 1, 1}});
  check(assembleSceneShader(plain) == fg::assembleTraceShader(plain, false),
        "a plain graph's shader is DualC's, unpatched");

  // The cost rule of the culling: analytic primitives are cheap, a TPMS is
  // costly; only costly objects get the box test.
  check(!low.objects[0].culled && low.objects[2].culled, "a sphere is cheap, a gyroid costly");
  check(src.find("sceneBoxDist(p, u_o4_boxMin, u_o4_boxMax)") != std::string::npos &&
            src.find("sceneBoxDist(p, u_o1_boxMin") == std::string::npos,
        "only the costly object tests its box");
  check(!isCostlyObject(parseGraphText(
            "union(sphere(radius=1),translate(by=[1,0,0],box(min=[0,0,0],max=[1,1,1])))", false)),
        "a small union of placed primitives is cheap");
  check(isCostlyObject(parseGraphText("onion(sphere(radius=1),thickness=0.1)", false)),
        "onion is costly");
  check(lowerScene(scene, false).scene.generatedSource.find("sceneBoxDist") == std::string::npos,
        "without culling no object tests its box");
  {
    Scene cheap;
    cheap.add("a", sphere);
    cheap.add("b", sphere).transform = dualc::Mat4::translation({2.0, 0.0, 0.0});
    check(lowerScene(cheap).scene.generatedSource ==
              lowerScene(cheap, false).scene.generatedSource,
          "a scene of cheap objects is P01's shader, byte for byte");
  }

  // An empty scene does not lower.
  threw = false;
  try {
    lowerScene(Scene());
  } catch (const fg::GraphError&) {
    threw = true;
  }
  check(threw, "an empty scene throws");

  if (failures) return 1;
  std::printf("scene_test: ok, %zu bindings, %zu chars of generated source\n",
              s.bindings.size(), src.size());
  return 0;
}
