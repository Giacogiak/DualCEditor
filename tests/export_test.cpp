// Mesh export (P06 T1), GL-free. Over a moved box filled with a gyroid
// (intersection(onion(normalize(gyroid)), box), the P05 T2 fill):
//  1. the format follows the extension;
//  2. the export graph is the object posed, with its parameter edits written
//     into the params; the visible scene is the union of its objects;
//  3. the fill exported at depth 6 to STL, 3MF and OBJ is watertight by
//     DualC's diagnostics, and the STL read back and welded is closed too;
//  4. the depth estimate is within 2x of the triangles DualC wrote;
//  5. the tiled STL (E-27) is watertight read back, its estimate holds one
//     tile, and its triangle count is the whole export's within 2%;
//  6. a cancel, requested before the start or from the progress sink in the
//     middle, leaves no file and no `.part`, and a file already at the path
//     is left as it was, whole and tiled;
//  7. an ExportJob runs on its own thread: the event loop turns while it runs,
//     its progress reaches the window's thread, and its cancel leaves no file;
//  8. the export dialog's fields give the spec, its tiled box follows the
//     format, and its estimate follows the depth.

#include "export_dialog.h"
#include "export_job.h"
#include "mesh_export.h"
#include "mesh_solid.h"
#include "scene_document.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>

#include <cmath>
#include <cstdio>
#include <string>

namespace fg = dce::fieldgraph;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
  std::fprintf(ok ? stdout : stderr, "export_test: %s %s\n", ok ? "ok  " : "FAIL",
               what.c_str());
  if (!ok) ++failures;
}

const fg::GraphNode* findOp(const fg::GraphNode& n, const std::string& op) {
  if (n.op == op) return &n;
  for (const fg::GraphNode& c : n.in)
    if (const fg::GraphNode* f = findOp(c, op)) return f;
  return nullptr;
}

QByteArray readAll(const QString& path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeText(const QString& path, const QByteArray& bytes) {
  QFile f(path);
  if (f.open(QIODevice::WriteOnly)) f.write(bytes);
}

// Requests its token at the first Contour report: a cancel in the middle.
struct CancelAtContour : dualc::ProgressSink {
  dualc::CancelToken* token = nullptr;
  bool seen = false;
  void report(dualc::Stage stage, std::size_t, std::size_t) noexcept override {
    if (stage == dualc::Stage::Contour || stage == dualc::Stage::Tile) {
      seen = true;
      token->request();
    }
  }
};

}  // namespace

int main(int argc, char** argv) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app(argc, argv);
  QTemporaryDir tmp;
  const QDir dir(tmp.path());

  // 1. The format.
  check(exportFormat("a.stl") == ExportFormat::Stl && exportFormat("b.STL") == ExportFormat::Stl &&
            exportFormat("c.3mf") == ExportFormat::ThreeMf &&
            exportFormat("d.obj") == ExportFormat::Obj &&
            exportFormat("e.ply") == ExportFormat::Unknown && exportFormat("f") == ExportFormat::Unknown,
        "the format follows the extension: .stl, .3mf, .obj, case-insensitive");
  check(exportTileable(ExportFormat::Stl) && exportTileable(ExportFormat::ThreeMf) &&
            !exportTileable(ExportFormat::Obj),
        "STL and 3MF tile, OBJ does not");

  // The scene: a 20 mm box moved by (30, 0, 0), filled with DualC's default
  // gyroid for its box (wavelength = diagonal / 10, about 3.5 mm).
  Scene scene;
  SceneObject& box = scene.add("box", fg::parseShorthand("box(min=[-10,-10,-10],max=[10,10,10])"));
  box.transform.m[0][3] = 30.0;
  const dualc::BBox world{{20, -10, -10}, {40, 10, 10}};
  const FillSpec spec = defaultFill("gyroid", world);
  SceneObject& fill = scene.add("fill", fillGraph(spec, *scene.find(1)));
  const int fillId = fill.id;
  // A third object, a gyroid source with an edited wavelength.
  SceneObject& edited =
      scene.add("edited", fg::parseShorthand("intersection(box(min=[-5,-5,-5],max=[5,5,5]),"
                                             "gyroid(wavelength=5))"));
  const fg::GraphNode* gy = findOp(edited.graph, "gyroid");
  edited.edits[{gy->pointer, "wavelength"}] = {7.0f};
  const int editedId = edited.id;

  // 2. The graph.
  std::size_t unbaked = 99;
  const fg::GraphNode one = exportGraph(scene, {editedId}, &unbaked);
  const fg::GraphNode* g = findOp(one, "gyroid");
  check(g && g->params.at("wavelength").num == 7.0 && unbaked == 0,
        "an object's parameter edit is written into the exported graph");
  const fg::GraphNode posed = exportGraph(scene, {1});
  check(posed.op == "transform" && posed.params.at("matrix").vec[3] == 30.0,
        "one object is exported posed: transform over its graph");
  const fg::GraphNode both = exportGraph(scene, {1, editedId});
  check(both.op == "union" && both.in.size() == 2 && exportGraph(scene, {}).op.empty(),
        "several objects are exported as their union; none gives an empty graph");

  std::shared_ptr<ExportField> field = buildExportField(exportGraph(scene, {fillId}), {});
  check(field->bounds.min.x < 20.0 && field->bounds.max.x > 40.0 && field->bounds.min.x > 18.0,
        "the default bounds are the fill's world box padded by 5%");

  // 3. Depth 6, whole, three formats.
  ExportSpec s;
  s.depth = 6;
  s.bounds = field->bounds;
  const ExportProbe probe = probeExport(field->field(), s.bounds, s.depth);
  std::size_t stlTriangles = 0;
  for (const char* ext : {"stl", "3mf", "obj"}) {
    s.path = dir.filePath(QString("fill.%1").arg(ext));
    QElapsedTimer t;
    t.start();
    const ExportResult r = runExport(field->field(), s);
    const double ms = double(t.nsecsElapsed()) / 1e6;
    std::printf("export_test: depth 6 %s: rc %d, %zu triangles, %zu open, %zu non-manifold, %.0f ms\n",
                ext, r.rc, r.diag.outputTriangles, r.diag.outputBoundaryEdges,
                r.diag.outputNonManifoldEdges, ms);
    check(r.status == ExportStatus::Written && r.diagnosed && !r.diag.emptyContour &&
              r.diag.outputWatertight && r.diag.outputTriangles > 1000 &&
              QFile::exists(s.path) && !QFile::exists(s.path + ".part"),
          std::string("the gyroid fill at depth 6 writes a watertight .") + ext +
              " (DualC's diagnostics)");
    if (std::string(ext) == "stl") stlTriangles = r.diag.outputTriangles;
  }
  const MeshImport back = importMeshFile(dir.filePath("fill.stl"));
  std::printf("export_test: fill.stl read back: %u triangles, %s\n", back.read.mesh->triangleCount(),
              back.check.summary().toStdString().c_str());
  check(back.error.isEmpty() && back.check.watertight() &&
            back.read.mesh->triangleCount() == stlTriangles,
        "the STL read back and welded is closed, with DualC's triangle count");

  // 4. The estimate.
  const ExportEstimate est = estimateExport(probe, s);
  const double ratio = est.faces / double(stlTriangles);
  std::printf("export_test: estimate at depth 6 from probe depth %d (%zu faces): %.0f, real %zu, "
              "ratio %.3f\n",
              probe.depth, probe.centroids.size(), est.faces, stlTriangles, ratio);
  check(probe.depth == 5 && ratio >= 0.5 && ratio <= 2.0,
        "the depth estimate is within 2x of the real face count");

  // 5. Tiled STL.
  ExportSpec tiled = s;
  tiled.path = dir.filePath("tiled.stl");
  tiled.tiled = true;
  tiled.tileDepth = 4;
  const ExportResult tr = runExport(field->field(), tiled);
  const MeshImport tback = importMeshFile(tiled.path);
  const ExportEstimate test = estimateExport(probe, tiled);
  std::printf("export_test: tiled depth 6 / tile 4: rc %d, %u triangles, %s; %d tiles per axis, "
              "busiest tile est. %.0f of %.0f faces\n",
              tr.rc, tback.read.mesh->triangleCount(), tback.check.summary().toStdString().c_str(),
              test.tiles, test.peakFaces, test.faces);
  check(tr.status == ExportStatus::Written && !tr.diagnosed && tback.check.watertight() &&
            std::abs(double(tback.read.mesh->triangleCount()) - double(stlTriangles)) <=
                0.02 * double(stlTriangles),
        "the tiled STL is watertight read back and holds the whole export's triangles");
  check(test.tiles == tilesPerAxis(6, 4) && test.tiles == 5 && test.peakFaces < 0.5 * test.faces &&
            test.ramBytes < est.ramBytes,
        "the tiled estimate holds one tile: less RAM than the whole");
  ExportSpec obj3 = tiled;
  obj3.path = dir.filePath("tiled.obj");
  const ExportResult notTiled = runExport(field->field(), obj3);
  check(notTiled.status == ExportStatus::Written && notTiled.diagnosed,
        "OBJ ignores tiled: it is written whole");
  ExportSpec bad = s;
  bad.path = dir.filePath("fill.ply");
  const ExportResult br = runExport(field->field(), bad);
  check(br.status == ExportStatus::Failed && !br.error.isEmpty() && !QFile::exists(bad.path),
        "an unknown extension fails with a message and writes nothing");

  // 6. Cancel.
  for (bool isTiled : {false, true}) {
    const std::string kind = isTiled ? "tiled" : "whole";
    ExportSpec c = isTiled ? tiled : s;
    c.path = dir.filePath(QString("cancel-%1.stl").arg(isTiled));
    dualc::CancelToken before;
    before.request();
    const ExportResult r0 = runExport(field->field(), c, &before);
    check(r0.status == ExportStatus::Cancelled && r0.rc == 3 && !QFile::exists(c.path) &&
              !QFile::exists(c.path + ".part"),
          kind + ": a cancel before the start leaves no file and no .part");
    dualc::CancelToken mid;
    CancelAtContour sink;
    sink.token = &mid;
    const ExportResult r1 = runExport(field->field(), c, &mid, &sink);
    check(sink.seen && r1.status == ExportStatus::Cancelled && !QFile::exists(c.path) &&
              !QFile::exists(c.path + ".part"),
          kind + ": a cancel in the middle leaves no file and no .part");
    writeText(c.path, "old bytes");
    dualc::CancelToken again;
    CancelAtContour sink2;
    sink2.token = &again;
    runExport(field->field(), c, &again, &sink2);
    check(readAll(c.path) == "old bytes" && !QFile::exists(c.path + ".part"),
          kind + ": a cancelled export leaves a file already there untouched");
  }

  // 7. ExportJob: a worker thread, the event loop running meanwhile.
  {
    ExportSpec j = s;
    j.depth = 6;
    j.path = dir.filePath("job.stl");
    ExportJob job(field, j);
    int ticks = 0, reports = 0, last = -1;
    bool monotonic = true;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, [&] { ++ticks; });
    QObject::connect(&job, &ExportJob::progress, [&](int p, const QString&) {
      ++reports;
      monotonic = monotonic && p >= last;
      last = p;
    });
    QEventLoop loop;
    QObject::connect(&job, &ExportJob::finished, &loop, &QEventLoop::quit);
    tick.start(5);
    job.start();
    loop.exec();
    std::printf("export_test: job depth 6: %.0f ms, %d timer ticks, %d progress reports, last %d\n",
                job.ms(), ticks, reports, last);
    check(job.result().status == ExportStatus::Written && QFile::exists(j.path) && ticks > 0 &&
              reports > 0 && monotonic && !job.running(),
          "an ExportJob writes on its thread while the event loop turns and progress arrives");

    ExportSpec k = j;
    k.path = dir.filePath("job-cancel.stl");
    ExportJob cancelled(field, k);
    QEventLoop loop2;
    QObject::connect(&cancelled, &ExportJob::progress, [&] { cancelled.cancel(); });
    QObject::connect(&cancelled, &ExportJob::finished, &loop2, &QEventLoop::quit);
    cancelled.start();
    loop2.exec();
    check(cancelled.result().status == ExportStatus::Cancelled && !QFile::exists(k.path) &&
              !QFile::exists(k.path + ".part"),
          "an ExportJob cancelled at its first progress report leaves no file");
  }

  // 8. The dialog.
  {
    ExportDialog d(field, "fill", dir.filePath("dialog.3mf"));
    d.setDepth(6);
    const ExportSpec ds = d.spec();
    check(ds.path.endsWith("dialog.3mf") && ds.depth == 6 && !ds.tiled &&
              std::abs(ds.bounds.min.x - field->bounds.min.x) < 1e-3 &&
              std::abs(ds.bounds.max.z - field->bounds.max.z) < 1e-3,
          "the dialog's spec holds its path, depth and the field's bounds");
    auto* tiledBox = d.findChild<QCheckBox*>("exportTiled");
    check(tiledBox && tiledBox->isEnabled(), "tiled is offered for 3MF");
    d.setTiled(true, 4);
    check(d.spec().tiled && d.spec().tileDepth == 4, "the tiled fields reach the spec");
    d.setPath(dir.filePath("dialog.obj"));
    check(!tiledBox->isEnabled() && !d.spec().tiled, "tiled is off for OBJ");
    d.setTiled(false, 4);
    const double at6 = d.estimate().faces;
    d.setDepth(7);
    const double at7 = d.estimate().faces;
    check(std::abs(at6 - est.faces) < 1e-6 && std::abs(at7 / at6 - 4.0) < 1.5,
          "the dialog's estimate is estimateExport's and grows with the depth");
  }

  std::printf("export_test: %s\n", failures ? "FAILED" : "all passed");
  return failures ? 1 : 0;
}
