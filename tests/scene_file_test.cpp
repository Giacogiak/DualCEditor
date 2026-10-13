// The .dcscene file without GL: a scene of a JSON graph, a shorthand graph
// reading a mesh beside it, a sphere and an operation over two of them, with
// a rename, a move, a hidden object, a colour and a parameter edit, is saved
// and loaded back. The loaded scene has the same objects, the same field
// values at sample points, the same lowered shader and uniform values, and the
// same mesh bytes. The file names no absolute path; moved to another folder,
// with the graphs and the mesh it was made from deleted, it still loads the
// same scene. A file of a future version, with no version or not a zip is
// refused with a message.

#include "graph_file.h"
#include "graph_scene.h"
#include "scene.h"
#include "scene_document.h"
#include "scene_file.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace fg = dce::fieldgraph;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "scene_file_test: FAIL %s\n", what);
    ++failures;
  }
}

constexpr const char* kCubeObj =
    "v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\n"
    "v -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
    "f 1 3 2\nf 1 4 3\nf 5 6 7\nf 5 7 8\nf 1 2 6\nf 1 6 5\n"
    "f 2 3 7\nf 2 7 6\nf 3 4 8\nf 3 8 7\nf 4 1 5\nf 4 5 8\n";
constexpr const char* kMeshFld = "intersection(mesh(path=\"cube.obj\"),sphere(radius=1.3))\n";

bool write(const QString& path, const QByteArray& bytes) {
  QFile f(path);
  return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
}

QByteArray read(const QString& path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// What the viewport would get: the shader source and every uniform value.
std::string fingerprint(const LoweredScene& l) {
  std::string out = l.scene.generatedSource;
  for (const fg::UniformBinding& b : l.scene.bindings) {
    out += "\n" + b.name + " =";
    for (float v : b.value) out += " " + std::to_string(v);
  }
  return out;
}

// Every object's field, in the world (its transform applied), at a grid of
// points over its world box: what the trace and the export sample.
std::vector<double> samples(const Scene& scene, const LoweredScene& lowered) {
  std::vector<double> out;
  for (std::size_t k = 0; k < scene.objects().size(); ++k) {
    const SceneObject& o = scene.objects()[k];
    fg::GraphNode t;
    t.op = "transform";
    fg::ParamValue m;
    m.kind = fg::ParamValue::Kind::Vector;
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) m.vec.push_back(o.transform.m[i][j]);
    t.params["matrix"] = m;
    t.in.push_back(o.graph);
    GraphDirMeshResolver meshes(o.meshDir);
    const fg::FieldGraph field = fg::FieldGraph::build(t, meshes);
    const dualc::BBox& b = lowered.objects[k].worldBounds;
    constexpr int n = 7;
    for (int x = 0; x < n; ++x)
      for (int y = 0; y < n; ++y)
        for (int z = 0; z < n; ++z) {
          const dualc::Vector3 p{b.min.x + (b.max.x - b.min.x) * x / (n - 1),
                                 b.min.y + (b.max.y - b.min.y) * y / (n - 1),
                                 b.min.z + (b.max.z - b.min.z) * z / (n - 1)};
          out.push_back(field.field().valueAt(p));
        }
  }
  return out;
}

// The baked mesh textures, byte for byte.
bool sameMeshTextures(const LoweredScene& a, const LoweredScene& b) {
  if (a.scene.meshes.size() != b.scene.meshes.size()) return false;
  for (std::size_t i = 0; i < a.scene.meshes.size(); ++i) {
    const std::vector<float>& x = a.scene.meshes[i].values;
    const std::vector<float>& y = b.scene.meshes[i].values;
    if (x.size() != y.size() || std::memcmp(x.data(), y.data(), x.size() * sizeof(float)) != 0)
      return false;
  }
  return true;
}

// The graph with every mesh path blanked: a saved scene renames them.
fg::GraphNode withoutMeshPaths(fg::GraphNode n) {
  if (n.op == "mesh" || n.op == "winding") n.params["path"].str.clear();
  for (fg::GraphNode& c : n.in) c = withoutMeshPaths(c);
  return n;
}

// The mesh paths a graph reads, in order.
void meshPaths(const fg::GraphNode& n, std::vector<std::string>* out) {
  if (n.op == "mesh" || n.op == "winding") out->push_back(n.params.at("path").str);
  for (const fg::GraphNode& c : n.in) meshPaths(c, out);
}

// Every field of every object but meshDir and file, and the graphs but their
// mesh paths.
bool sameObjects(const Scene& a, const Scene& b) {
  if (a.objects().size() != b.objects().size()) return false;
  for (std::size_t i = 0; i < a.objects().size(); ++i) {
    const SceneObject& x = a.objects()[i];
    const SceneObject& y = b.objects()[i];
    if (x.id != y.id || x.name != y.name || x.visible != y.visible || x.colour != y.colour ||
        x.edits != y.edits || x.operands != y.operands ||
        std::memcmp(x.transform.m, y.transform.m, sizeof(x.transform.m)) != 0 ||
        !sameGraph(withoutMeshPaths(x.graph), withoutMeshPaths(y.graph)))
      return false;
  }
  return true;
}

QString loadError(const QString& path) {
  const SceneLoad load = loadSceneFile(path);
  check(!load.scene, "a refused file gives no scene");
  return load.error;
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  if (argc != 2) {
    std::fprintf(stderr, "usage: scene_file_test <gyroid_box.json>\n");
    return 2;
  }
  QTemporaryDir tmp;
  if (!tmp.isValid()) return 2;
  const QString src = tmp.filePath("src");
  QDir().mkpath(src);
  const QString gyroidFile = src + "/gyroid_box.json";
  const QString meshFile = src + "/mesh.fld";
  if (!write(gyroidFile, read(QString::fromLocal8Bit(argv[1]))) ||
      !write(src + "/cube.obj", kCubeObj) || !write(meshFile, kMeshFld)) {
    std::fprintf(stderr, "scene_file_test: cannot write the sources\n");
    return 1;
  }

  SceneDocument doc;
  const int gyroid = doc.addObject("gyroid_box.json", *parseGraphFile(gyroidFile).graph,
                                   graphMeshDir(gyroidFile), gyroidFile);
  const int mesh =
      doc.addObject("mesh.fld", *parseGraphFile(meshFile).graph, graphMeshDir(meshFile), meshFile);
  const int ball = doc.addObject("ball", parseGraphText("sphere(radius=0.75)", false));
  doc.renameObject(gyroid, "lattice");
  doc.setTransform(gyroid, dualc::Mat4::translation({2.5, 0.0, 0.0}) *
                               dualc::Mat4::rotation({0.0, 0.0, 1.0}, 0.3));
  doc.setTransform(ball, dualc::Mat4::translation({0.0, 1.0, 0.0}));
  doc.setColour(mesh, {0.1f, 0.2f, 0.3f});
  // A parameter edit of the gyroid: a binding that is not the object's own.
  {
    const LoweredScene l = lowerScene(doc.scene());
    const LoweredObject& lo = l.objects[0];
    for (int i = lo.firstBinding; i < lo.endBinding; ++i) {
      const fg::UniformBinding& b = l.scene.bindings[std::size_t(i)];
      if (b.paramKey != "wavelength") continue;
      std::vector<float> v = b.value;
      v[0] *= 0.8f;
      doc.setParameter(gyroid, {b.nodePointer, b.paramKey}, v, b.value);
      break;
    }
  }
  check(!doc.scene().find(gyroid)->edits.empty(), "the gyroid has a parameter edit");
  // An operation over the mesh and the ball: the mesh path becomes absolute
  // (the operands resolve meshes in different folders), the same mesh twice.
  const int both = doc.applyOperation("union", {}, {mesh, ball});
  doc.setVisible(gyroid, false);
  check(both != 0 && !doc.scene().find(mesh)->visible, "the operation hides its operands");
  {
    std::vector<std::string> paths;
    meshPaths(doc.scene().find(both)->graph, &paths);
    check(paths.size() == 1 && QDir::isAbsolutePath(QString::fromStdString(paths[0])),
          "the operation's mesh path is absolute");
  }

  const Scene original = doc.scene();
  const LoweredScene originalLowered = lowerScene(original);
  const std::vector<double> originalSamples = samples(original, originalLowered);

  // Save, and what the file holds.
  QDir().mkpath(tmp.filePath("a"));
  const QString saved = tmp.filePath("a/scene.dcscene");
  check(saveSceneFile(original, saved).isEmpty(), "the scene saves");
  QString error;
  const auto entries = readZip(read(saved), &error);
  check(entries.has_value(), "the file is a zip");
  if (!entries) return 1;
  check(entries->size() == 2 && entries->count("scene.json") && entries->count("meshes/0.obj"),
        "scene.json and one mesh entry: the mesh read twice is stored once");
  check(entries->count("meshes/0.obj") && entries->at("meshes/0.obj") == QByteArray(kCubeObj),
        "the stored mesh is the file's bytes");
  const QByteArray json = entries->count("scene.json") ? entries->at("scene.json") : QByteArray();
  const QJsonObject root = QJsonDocument::fromJson(json).object();
  check(root["format"].toString() == "dcscene" && root["version"].toInt() == kSceneFileVersion,
        "format and version");
  check(!json.contains(tmp.path().toUtf8()) && !json.contains(QDir::homePath().toUtf8()),
        "scene.json names no absolute path");
  check(json.contains("\"file\": \"../src/gyroid_box.json\""),
        "an object's file is relative to the scene's folder");

  // Load: the same scene.
  SceneLoad load = loadSceneFile(saved);
  check(load.scene.has_value(), "the scene loads");
  if (!load.scene) {
    std::fprintf(stderr, "scene_file_test: %s\n", qPrintable(load.error));
    return 1;
  }
  const Scene& loaded = *load.scene;
  check(sameObjects(original, loaded), "the same objects, flags, colours, edits, transforms");
  check(loaded.find(gyroid)->file == gyroidFile && loaded.find(mesh)->file == meshFile &&
            loaded.find(ball)->file.isEmpty(),
        "files are found again relative to the scene");
  const LoweredScene loadedLowered = lowerScene(loaded);
  check(fingerprint(loadedLowered) == fingerprint(originalLowered),
        "the same shader and uniform values");
  check(samples(loaded, loadedLowered) == originalSamples, "the same field values");
  check(sameMeshTextures(loadedLowered, originalLowered), "the same baked mesh textures");
  {
    std::vector<std::string> paths;
    meshPaths(loaded.find(both)->graph, &paths);
    meshPaths(loaded.find(mesh)->graph, &paths);
    check(paths.size() == 2 && paths[0] == "meshes/0.obj" && paths[1] == "meshes/0.obj",
          "mesh paths name the entry");
    check(read(QDir(loaded.find(mesh)->meshDir).filePath("meshes/0.obj")) ==
              QByteArray(kCubeObj),
          "the extracted mesh is the file's bytes");
  }

  // Moved to another folder, its sources deleted: it still loads the same.
  const QString moved = tmp.filePath("b/c/moved.dcscene");
  QDir().mkpath(tmp.filePath("b/c"));
  check(QFile::copy(saved, moved), "copy the scene");
  QDir(src).removeRecursively();
  QDir(tmp.filePath("a")).removeRecursively();
  load = loadSceneFile(moved);
  check(load.scene.has_value(), "the moved scene loads");
  if (load.scene) {
    check(sameObjects(original, *load.scene), "the moved scene has the same objects");
    for (const SceneObject& o : load.scene->objects())
      check(o.file.isEmpty(), "a file no longer there is dropped");
    const LoweredScene l = lowerScene(*load.scene);
    check(fingerprint(l) == fingerprint(originalLowered), "moved: the same shader and values");
    check(samples(*load.scene, l) == originalSamples, "moved: the same field values");
    check(sameMeshTextures(l, originalLowered), "moved: the same baked mesh textures");
    // Saved again, it holds the same mesh bytes and graphs.
    const QString again = tmp.filePath("b/again.dcscene");
    check(saveSceneFile(*load.scene, again).isEmpty(), "the loaded scene saves again");
    const auto entries2 = readZip(read(again), &error);
    check(entries2 && entries2->size() == 2 && entries2->count("meshes/0.obj") &&
              entries2->at("meshes/0.obj") == QByteArray(kCubeObj),
          "saved again: the same mesh entry");
    const SceneLoad reload = loadSceneFile(again);
    check(reload.scene && sameObjects(original, *reload.scene), "saved again: the same objects");
  }

  // Refused files.
  const QString future = tmp.filePath("future.dcscene");
  QJsonObject next = root;
  next["version"] = kSceneFileVersion + 1;
  check(writeZip(future, {{"scene.json", QJsonDocument(next).toJson()}}).isEmpty(),
        "write a future file");
  error = loadError(future);
  std::printf("scene_file_test: future version: %s\n", qPrintable(error));
  check(error.contains(QString("version %1").arg(kSceneFileVersion + 1)) &&
            error.contains("newer"),
        "a future version is refused, naming it");
  QJsonObject none = root;
  none.remove("version");
  writeZip(future, {{"scene.json", QJsonDocument(none).toJson()}});
  check(loadError(future).contains("no scene version"), "no version is refused");
  write(future, "not a zip at all");
  check(loadError(future).contains("not a zip"), "a file that is no zip is refused");
  QJsonObject evil = root;
  evil["meshes"] = QJsonArray{"meshes/../../evil.obj"};
  writeZip(future, {{"scene.json", QJsonDocument(evil).toJson()},
                    {"meshes/../../evil.obj", "x"}});
  check(loadError(future).contains("not in the archive"),
        "a mesh entry outside meshes/ is refused");

  // An empty scene round-trips.
  const QString empty = tmp.filePath("empty.dcscene");
  check(saveSceneFile(Scene{}, empty).isEmpty(), "an empty scene saves");
  load = loadSceneFile(empty);
  check(load.scene && load.scene->isEmpty(), "an empty scene loads");

  if (failures) return 1;
  std::printf("scene_file_test: ok\n");
  return 0;
}
