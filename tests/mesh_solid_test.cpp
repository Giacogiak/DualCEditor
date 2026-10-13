// Import as a solid (P04 T2), GL-free: a closed, outward cube becomes a `mesh`
// node; an open box, an inside-out cube and a cube with one flipped triangle
// become `winding` nodes, each with DualC's edge counts and our orientation
// facts. "Make watertight" contours the open box into a closed, outward mesh,
// reports the sample and contour stages, and stops with dualc::Cancelled when
// its token is requested during the contour. A .dcscene carries an imported
// mesh's buffers and reads them back under the same id.

#include "mesh_import.h"
#include "mesh_library.h"
#include "mesh_solid.h"
#include "scene.h"
#include "scene_file.h"

#include <QString>
#include <QTemporaryDir>

#include <cstdio>
#include <utility>

namespace {

int failures = 0;

void check(bool ok, const QString& what) {
  std::fprintf(ok ? stdout : stderr, "mesh_solid_test: %s %s\n", ok ? "ok  " : "FAIL",
               what.toUtf8().constData());
  if (!ok) ++failures;
}

// Counts the reports of each stage; requests `cancel` at the first report of
// `cancelAt`, when given.
struct Sink : dualc::ProgressSink {
  int reports[4] = {0, 0, 0, 0};
  bool ended[4] = {false, false, false, false};
  dualc::CancelToken* cancel = nullptr;
  int cancelAt = -1;
  void report(dualc::Stage stage, std::size_t done, std::size_t total) noexcept override {
    ++reports[int(stage)];
    ended[int(stage)] = done == total;
    if (cancel && int(stage) == cancelAt) cancel->request();
  }
};

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: mesh_solid_test <tests/data/meshes>\n");
    return 2;
  }
  const MeshRead read = readMeshFile(QString(argv[1]) + "/cube_ascii.stl");
  check(bool(read.mesh), "the cube fixture reads");
  if (!read.mesh) return 1;
  const MeshBuffers cube = *read.mesh;

  const MeshCheck closed = checkMesh(cube);
  check(closed.solid() && closed.summary() == "watertight",
        QString("the closed cube is watertight and oriented: %1, volume %2")
            .arg(closed.summary())
            .arg(closed.volume));
  check(closed.volume > 7999.0 && closed.volume < 8001.0, "its volume is 20^3 mm^3");

  // Two triangles off: one face open, four boundary edges.
  MeshBuffers open = cube;
  open.indices.resize(open.indices.size() - 6);
  open = weldVertices(open.vertices, open.indices);
  const MeshCheck openCheck = checkMesh(open);
  check(!openCheck.solid() && openCheck.diag.inputBoundaryEdges == 4 &&
            !openCheck.diag.inputWatertight,
        "the open box has 4 open edges: " + openCheck.summary());

  MeshBuffers inside = cube;
  for (std::size_t t = 0; t < inside.indices.size(); t += 3)
    std::swap(inside.indices[t + 1], inside.indices[t + 2]);
  const MeshCheck insideCheck = checkMesh(inside);
  check(insideCheck.watertight() && !insideCheck.oriented() && insideCheck.volume < 0.0 &&
            insideCheck.summary() == "normals inward",
        "the inside-out cube is watertight, not oriented: " + insideCheck.summary());

  MeshBuffers flipped = cube;
  std::swap(flipped.indices[1], flipped.indices[2]);
  const MeshCheck flippedCheck = checkMesh(flipped);
  check(flippedCheck.watertight() && flippedCheck.misorientedEdges == 3 &&
            !flippedCheck.solid(),
        "one flipped triangle gives 3 misoriented edges: " + flippedCheck.summary());

  check(checkMesh(MeshBuffers{}).diag.inputEmpty && !checkMesh(MeshBuffers{}).solid(),
        "an empty mesh is not a solid");

  // The node each becomes.
  const MeshImport solid = importMeshBuffers(cube);
  const MeshImport soup = importMeshBuffers(open);
  check(solid.graph && solid.graph->op == "mesh" && meshObjectKind(*solid.graph) == "mesh" &&
            findLibraryMesh(solid.id),
        "the closed cube imports as mesh(id=...)");
  check(soup.graph && soup.graph->op == "winding" && meshObjectKind(*soup.graph) == "winding",
        "the open box imports as winding(id=...)");
  dce::fieldgraph::GraphNode sphere;
  sphere.op = "sphere";
  check(meshObjectKind(sphere).isEmpty(), "a sphere has no mesh kind");
  const MeshImport missing = importMeshFile(QString(argv[1]) + "/no_such.obj");
  check(!missing.graph && !missing.error.isEmpty(), "a missing file is an error: " + missing.error);

  // Make watertight: the open box closes, outward, with both stages reported.
  {
    Sink sink;
    dualc::Diagnostics diag;
    const MeshBuffers sealed = makeWatertight(open, 6, nullptr, &sink, &diag);
    const MeshCheck c = checkMesh(sealed);
    check(c.solid(), QString("make watertight closes the open box: %1 vertices, %2 triangles, %3")
                         .arg(sealed.vertexCount())
                         .arg(sealed.triangleCount())
                         .arg(c.summary()));
    check(sink.reports[int(dualc::Stage::Sample)] > 0 && sink.ended[int(dualc::Stage::Sample)] &&
              sink.reports[int(dualc::Stage::Contour)] > 0 &&
              sink.ended[int(dualc::Stage::Contour)],
          "it reports the sample and the contour stages, each to its end");
  }
  // Cancelled at the first contour report: DualC throws Cancelled.
  {
    Sink sink;
    dualc::CancelToken token;
    sink.cancel = &token;
    sink.cancelAt = int(dualc::Stage::Contour);
    bool cancelled = false;
    try {
      makeWatertight(open, 6, &token, &sink);
    } catch (const dualc::Cancelled&) {
      cancelled = true;
    }
    check(cancelled, "a token requested during the contour stops it with Cancelled");
  }

  // Saved and loaded: the archive holds the buffers as meshes/<id>.obj, and
  // the read gives the same id (a missing entry would be a load error).
  {
    QTemporaryDir tmp;
    Scene scene;
    scene.add("cube.stl", *solid.graph);
    scene.add("open.obj", *soup.graph);
    const QString path = tmp.filePath("imported.dcscene");
    const QString error = saveSceneFile(scene, path);
    check(error.isEmpty(), "a scene of imported meshes saves " + error);
    const SceneLoad load = loadSceneFile(path);
    check(load.scene && load.scene->objects().size() == 2 &&
              load.scene->objects()[0].graph.op == "mesh" &&
              load.scene->objects()[0].graph.params.at("id").str == solid.id &&
              load.scene->objects()[1].graph.op == "winding" &&
              load.scene->objects()[1].graph.params.at("id").str == soup.id,
          "it loads with the same kinds and ids " + load.error);
  }

  std::printf("mesh_solid_test: %s\n", failures ? "FAILED" : "passed");
  return failures ? 1 : 0;
}
