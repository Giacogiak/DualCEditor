#include "scene.h"

#include "graph_file.h"
#include "graph_scene.h"

#include <algorithm>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace fg = dce::fieldgraph;
using dualc::BBox;
using dualc::Vector3;

namespace {

// The one line of DualC's kTraceFramework that sets the surface colour.
constexpr const char* kBaseAnchor = "vec3  base = vec3(0.80, 0.82, 0.86);";
constexpr const char* kSceneColorDecl = "vec3 sceneColor(vec3 p)";

// The axis-aligned box around `b` moved by `m`.
BBox transformBox(const BBox& b, const dualc::Mat4& m) {
  const double inf = std::numeric_limits<double>::infinity();
  BBox out{{inf, inf, inf}, {-inf, -inf, -inf}};
  for (int i = 0; i < 8; ++i) {
    const Vector3 c{(i & 1) ? b.max.x : b.min.x, (i & 2) ? b.max.y : b.min.y,
                    (i & 4) ? b.max.z : b.min.z};
    const Vector3 w = m.transformPoint(c);
    out.min = {std::min(out.min.x, w.x), std::min(out.min.y, w.y), std::min(out.min.z, w.z)};
    out.max = {std::max(out.max.x, w.x), std::max(out.max.y, w.y), std::max(out.max.z, w.z)};
  }
  return out;
}

BBox unite(const BBox& a, const BBox& b) {
  return {{std::min(a.min.x, b.min.x), std::min(a.min.y, b.min.y), std::min(a.min.z, b.min.z)},
          {std::max(a.max.x, b.max.x), std::max(a.max.y, b.max.y), std::max(a.max.z, b.max.z)}};
}

// Put object `id` into every identifier DualC's codegen names: its functions
// fN, its uniforms u_nN_* (mesh samplers and their _texMin/_texScale/_texDim
// included) and its sceneSDF.
struct Namespacer {
  explicit Namespacer(int id)
      : fn("o" + std::to_string(id) + "_f$1"),
        uniform("u_o" + std::to_string(id) + "_n$1_"),
        sdf("o" + std::to_string(id) + "_sdf") {}

  std::string operator()(const std::string& s) const {
    static const std::regex kFn(R"(\bf([0-9]+)\b)");
    static const std::regex kUniform(R"(\bu_n([0-9]+)_)");
    static const std::regex kSdf(R"(\bsceneSDF\b)");
    std::string out = std::regex_replace(s, kFn, fn);
    out = std::regex_replace(out, kUniform, uniform);
    return std::regex_replace(out, kSdf, sdf);
  }

  std::string fn, uniform, sdf;
};

fg::GraphNode wrapInTransform(const SceneObject& o) {
  fg::GraphNode t;
  t.op = "transform";
  fg::ParamValue matrix;
  matrix.kind = fg::ParamValue::Kind::Vector;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) matrix.vec.push_back(o.transform.m[i][j]);
  t.params["matrix"] = std::move(matrix);
  t.in.push_back(o.graph);
  t.pointer = "/objects/" + std::to_string(o.id);
  return t;
}

std::string glslVec3(const SceneColour& c) {
  std::ostringstream s;
  s.precision(9);
  s << "vec3(" << c[0] << ", " << c[1] << ", " << c[2] << ")";
  return s.str();
}

}  // namespace

SceneObject& Scene::add(QString name, fg::GraphNode graph, QString meshDir) {
  SceneObject o;
  o.id = nextId_++;
  o.name = std::move(name);
  o.graph = std::move(graph);
  o.meshDir = std::move(meshDir);
  o.colour = sceneColourFor(o.id);
  objects_.push_back(std::move(o));
  return objects_.back();
}

bool Scene::remove(int id) {
  const auto it = std::find_if(objects_.begin(), objects_.end(),
                               [id](const SceneObject& o) { return o.id == id; });
  if (it == objects_.end()) return false;
  objects_.erase(it);
  return true;
}

SceneObject* Scene::find(int id) {
  for (SceneObject& o : objects_)
    if (o.id == id) return &o;
  return nullptr;
}

const SceneObject* Scene::find(int id) const {
  return const_cast<Scene*>(this)->find(id);
}

SceneColour sceneColourFor(int id) {
  static constexpr SceneColour kPalette[] = {
      {0.85f, 0.42f, 0.25f},  // orange
      {0.30f, 0.55f, 0.85f},  // blue
      {0.42f, 0.72f, 0.35f},  // green
      {0.85f, 0.72f, 0.30f},  // yellow
      {0.65f, 0.42f, 0.80f},  // purple
      {0.35f, 0.75f, 0.75f},  // teal
      {0.85f, 0.45f, 0.60f},  // pink
  };
  constexpr int n = int(sizeof(kPalette) / sizeof(kPalette[0]));
  if (id <= 1) return kDualCBase;
  return kPalette[(id - 2) % n];
}

std::string objectUniformName(int id, const char* key) {
  return "u_o" + std::to_string(id) + "_" + key;
}

LoweredScene lowerScene(const Scene& scene) {
  if (scene.isEmpty()) throw fg::GraphError("the scene has no objects", "");
  LoweredScene out;
  fg::GlslScene& s = out.scene;
  s.stepScale = 1.0f;
  s.featureScale = 0.0f;
  std::ostringstream src, sdf, colour;
  sdf << "float sceneSDF(vec3 p) {\n  float d = 1e10;\n";
  colour << kSceneColorDecl << " {\n  float d = 1e10;\n  float id = 0.0;\n  vec3 c = "
         << glslVec3(kDualCBase) << ";\n";

  bool first = true;
  for (const SceneObject& o : scene.objects()) {
    fg::GlslScene part;
    LoweredObject lo;
    lo.id = o.id;
    try {
      GraphDirMeshResolver meshes(o.meshDir);
      lo.localBounds = traceBounds(o.graph, meshes);
      part = fg::compileToGlsl(wrapInTransform(o), meshes, lo.localBounds);
    } catch (const fg::GraphError& e) {
      throw fg::GraphError("object " + std::to_string(o.id) + " (" + o.name.toStdString() +
                               "): " + e.what(),
                           e.pointer());
    }
    lo.worldBounds = transformBox(lo.localBounds, o.transform);
    lo.stepScale = part.stepScale;
    lo.featureScale = part.featureScale;
    s.bounds = first ? lo.worldBounds : unite(s.bounds, lo.worldBounds);
    s.stepScale = std::min(s.stepScale, part.stepScale);
    if (part.featureScale > 0.0f)
      s.featureScale = s.featureScale > 0.0f ? std::min(s.featureScale, part.featureScale)
                                             : part.featureScale;
    s.nodeCount += part.nodeCount;
    first = false;

    const Namespacer rename(o.id);
    const std::string visible = objectUniformName(o.id, "visible");
    const std::string col = objectUniformName(o.id, "colour");
    src << "// object " << o.id << "\n" << rename(part.generatedSource);
    src << "uniform float " << visible << ";\nuniform vec3 " << col << ";\n\n";
    sdf << "  if (" << visible << " > 0.5) d = min(d, o" << o.id << "_sdf(p));\n";
    colour << "  if (" << visible << " > 0.5) { float di = o" << o.id
           << "_sdf(p); if (di < d) { d = di; c = " << col << "; id = " << o.id
           << ".0; } }\n";

    lo.firstBinding = int(s.bindings.size());
    const std::string pointer = "/objects/" + std::to_string(o.id);
    for (fg::UniformBinding& b : part.bindings) {
      b.name = rename(b.name);
      // The wrapper's matrix: the one binding of the node we made.
      if (b.nodePointer == pointer && b.glslType == "mat4")
        lo.transformBinding = int(s.bindings.size());
      s.bindings.push_back(std::move(b));
    }
    lo.visibleBinding = int(s.bindings.size());
    s.bindings.push_back({visible, "float", {o.visible ? 1.0f : 0.0f}, pointer, "visible"});
    lo.colourBinding = int(s.bindings.size());
    s.bindings.push_back(
        {col, "vec3", {o.colour[0], o.colour[1], o.colour[2]}, pointer, "colour"});
    lo.endBinding = int(s.bindings.size());

    for (fg::MeshTexture& m : part.meshes) {
      m.samplerName = rename(m.samplerName);
      s.meshes.push_back(std::move(m));
    }
    out.objects.push_back(lo);
  }
  sdf << "  return d;\n}\n";
  colour << "  if (" << kSelectedUniform << " > 0.5)\n    c = abs(id - " << kSelectedUniform
         << ") < 0.5 ? mix(c, " << glslVec3(kSelectionTint) << ", 0.45) : c * 0.45;\n"
         << "  return c;\n}\n";
  src << "uniform float " << kSelectedUniform << ";\n";
  s.generatedSource = src.str() + sdf.str() + colour.str();
  return out;
}

std::string assembleSceneShader(const fg::GlslScene& scene) {
  std::string fs = fg::assembleTraceShader(scene, false);
  if (scene.generatedSource.find(kSceneColorDecl) == std::string::npos) return fs;
  const std::size_t at = fs.find(kBaseAnchor);
  if (at == std::string::npos || fs.find(kBaseAnchor, at + 1) != std::string::npos)
    throw std::runtime_error(
        std::string("DualC's trace framework changed: the line \"") + kBaseAnchor +
        "\" the scene colour replaces is not there exactly once");
  fs.replace(at, std::char_traits<char>::length(kBaseAnchor),
             "vec3  base = sceneColor(hp);");
  return fs;
}
