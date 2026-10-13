// Drives a live MainWindow the three ways a graph comes in, and the file
// watch: a drop of a .json and of a .fld renders it (the second drop adds an
// object; the first is then removed); a drag of another file
// type is refused; saving the file (in place, or by write-and-rename) re-renders
// it with the camera kept; a save with the same text repaints nothing; a broken
// file shows its error with its JSON pointer and leaves the image in place; a
// shader that fails to link does the same. Then the command-line argument,
// through `DualCEditor --snapshot`.
//
// Needs a display and an OpenGL 3.3 core context; exits 77 (ctest: skipped)
// without them.
//
// usage: main_window_test <DualCEditor> <gyroid_box.json>

#include "main_window.h"
#include "objects_panel.h"
#include "transform_panel.h"
#include "viewport.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMimeData>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QTimer>
#include <QSet>
#include <QStatusBar>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QUrl>
#include <QWindow>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <algorithm>

namespace {

constexpr int kExitSkip = 77;
int failures = 0;

void check(bool ok, const char* what) {
  std::fprintf(ok ? stdout : stderr, "main_window_test: %s %s\n", ok ? "ok  " : "FAIL",
               what);
  if (!ok) ++failures;
}

void check(bool ok, const std::string& what) {
  check(ok, what.c_str());
}

template <class Pred>
bool runUntil(Pred done, int timeoutMs) {
  QElapsedTimer t;
  t.start();
  while (!done()) {
    if (t.elapsed() > timeoutMs) return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  }
  return true;
}

void idle(int ms) {
  runUntil([] { return false; }, ms);
}

// Wait until no paint has come for 500 ms.
void quiet(Viewport& vp) {
  int before = -1;
  runUntil(
      [&] {
        if (vp.paintCount() == before) return true;
        before = vp.paintCount();
        idle(500);
        return false;
      },
      20000);
}

void write(const QString& path, const QByteArray& text) {
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    std::fprintf(stderr, "main_window_test: cannot write %s\n", qPrintable(path));
    std::exit(1);
  }
  f.write(text);
}

QByteArray read(const QString& path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Send a drag-enter and, when accepted, a drop of `path` onto the window.
bool drop(MainWindow& w, const QString& path) {
  QMimeData mime;
  mime.setUrls({QUrl::fromLocalFile(path)});
  const QPoint at(w.width() / 2, w.height() / 2);
  QDragEnterEvent enter(at, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(&w, &enter);
  if (!enter.isAccepted()) return false;
  QDropEvent dropped(QPointF(at), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(&w, &dropped);
  return dropped.isAccepted();
}

// The outcome of the next load attempt, or -1 if none comes in `ms`.
int nextLoad(MainWindow& w, int ms = 10000) {
  int result = -1;
  const QMetaObject::Connection c =
      QObject::connect(&w, &MainWindow::loadFinished, [&](bool ok) { result = ok; });
  runUntil([&] { return result != -1; }, ms);
  QObject::disconnect(c);
  return result;
}

bool sameCamera(const OrbitCamera& a, const OrbitCamera& b) {
  return a.yaw == b.yaw && a.pitch == b.pitch && a.dist == b.dist &&
         a.target.x == b.target.x && a.target.y == b.target.y && a.target.z == b.target.z;
}

int runEditor(const QString& editor, const QStringList& args) {
  QProcess proc;
  proc.setProcessChannelMode(QProcess::ForwardedChannels);
  proc.start(editor, args);
  if (!proc.waitForStarted() || !proc.waitForFinished(600000) ||
      proc.exitStatus() != QProcess::NormalExit)
    return -1;
  return proc.exitCode();
}

// A unit cube, for a mesh node whose relative path sits beside the graph.
constexpr const char* kCubeObj =
    "v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\n"
    "v -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
    "f 1 3 2\nf 1 4 3\nf 5 6 7\nf 5 7 8\nf 1 2 6\nf 1 6 5\n"
    "f 2 3 7\nf 2 7 6\nf 3 4 8\nf 3 8 7\nf 4 1 5\nf 4 5 8\n";
constexpr const char* kMeshFld = "intersection(mesh(path=\"cube.obj\"),sphere(radius=1.3))\n";

constexpr const char* kFld =
    "difference(box(min=[-1,-1,-1],max=[1,1,1]),sphere(radius=1.25))\n";
constexpr const char* kFldEdited =
    "difference(box(min=[-1.5,-1.5,-1.5],max=[1.5,1.5,1.5]),sphere(radius=1.8))\n";
constexpr const char* kFldRenamed =
    "difference(box(min=[-1.5,-1.5,-1.5],max=[1.5,1.5,1.5]),sphere(radius=1.6))\n";

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: main_window_test <DualCEditor> <gyroid_box.json>\n");
    return 2;
  }
#ifdef Q_OS_LINUX
  if (qEnvironmentVariableIsEmpty("DISPLAY") &&
      qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
    std::printf("main_window_test: skipped, no display\n");
    return kExitSkip;
  }
#endif
  QSurfaceFormat format;
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(format);
  QApplication app(argc, argv);
  const QString platform = QGuiApplication::platformName();
  if (platform == "offscreen" || platform == "minimal") {
    std::printf("main_window_test: skipped, platform %s has no window GL\n",
                qPrintable(platform));
    return kExitSkip;
  }
  const QString editor = QString::fromLocal8Bit(argv[1]);
  const QByteArray gyroid = read(QString::fromLocal8Bit(argv[2]));
  if (gyroid.isEmpty()) {
    std::fprintf(stderr, "main_window_test: cannot read %s\n", argv[2]);
    return 1;
  }

  QTemporaryDir tmp;
  const QString json = tmp.filePath("gyroid_box.json");
  const QString fld = tmp.filePath("cube.fld");
  const QString txt = tmp.filePath("notes.txt");
  write(json, gyroid);
  write(fld, kFld);
  write(txt, "not a graph");

  // Preview off: every frame is a full one, so images compare exactly.
  MainWindow w(1.0f);
  w.resize(640, 480);
  w.show();
  Viewport& vp = *w.viewport();
  if (!runUntil([&] { return w.windowHandle() && w.windowHandle()->isExposed() &&
                             vp.paintCount() > 0; },
                10000)) {
    std::fprintf(stderr, "main_window_test: the window never painted\n");
    return 1;
  }
  if (!vp.isGLReady()) {
    std::printf("main_window_test: skipped, no OpenGL 3.3 core context\n");
    return kExitSkip;
  }
  check(!vp.hasScene(), "the window opens with no graph");

  // Drag and drop.
  check(!drop(w, txt), "a drag of a .txt is refused");
  check(drop(w, json), "a drop of a .json is accepted");
  check(nextLoad(w) == 1 || vp.hasScene(), "the dropped .json loads");
  quiet(vp);
  check(vp.hasScene() && vp.scene().nodeCount > 0, "the .json scene is on screen");
  check(w.currentFile() == QFileInfo(json).absoluteFilePath(), "the .json is current");
  const QImage gyroidImage = vp.grabFramebuffer();
  QSet<QRgb> colours;
  for (int y = 0; y < gyroidImage.height(); y += 4)
    for (int x = 0; x < gyroidImage.width(); x += 4) colours.insert(gyroidImage.pixel(x, y));
  check(colours.size() > 16, "the .json renders geometry");

  check(drop(w, fld), "a drop of a .fld is accepted");
  quiet(vp);
  check(w.currentFile() == QFileInfo(fld).absoluteFilePath() &&
            w.statusBar()->currentMessage().startsWith("Loaded cube.fld"),
        "the .fld loads");
  const QImage cubeImage = vp.grabFramebuffer();
  check(cubeImage != gyroidImage, "the .fld renders a different image");
  // A drop adds (P01 T2): the gyroid is still there and hides the cube inside
  // it. Remove it, so the cube's edits below show.
  check(w.scene().objects().size() == 2, "the drop added the .fld as a second object");
  w.removeObject(w.scene().objects().front().id);
  quiet(vp);
  check(w.scene().objects().size() == 1 && w.scene().objects().front().name == "cube.fld",
        "removing the gyroid leaves the cube");
  const OrbitCamera framed = vp.camera();

  // Orbit away, then save an edit: the image changes, the camera stays.
  {
    const QPointF c(vp.width() / 2.0, vp.height() / 2.0);
    QMouseEvent press(QEvent::MouseButtonPress, c, vp.mapToGlobal(c), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&vp, &press);
    const QPointF to = c + QPointF(40, 10);
    QMouseEvent move(QEvent::MouseMove, to, vp.mapToGlobal(to), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&vp, &move);
    QMouseEvent up(QEvent::MouseButtonRelease, to, vp.mapToGlobal(to), Qt::LeftButton,
                   Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&vp, &up);
  }
  quiet(vp);
  const OrbitCamera orbited = vp.camera();
  check(!sameCamera(orbited, framed), "the drag moved the camera");
  const QImage orbitedImage = vp.grabFramebuffer();

  QElapsedTimer saved;
  saved.start();
  write(fld, kFldEdited);
  check(nextLoad(w) == 1, "saving the .fld in place reloads it");
  const qint64 reloadMs = saved.elapsed();
  quiet(vp);
  check(sameCamera(vp.camera(), orbited), "the reload keeps the camera");
  const QImage editedImage = vp.grabFramebuffer();
  check(editedImage != orbitedImage, "the reload re-renders");
  std::printf("main_window_test: in-place save to reload %lld ms\n",
              static_cast<long long>(reloadMs));

  // The same text again (a save with no edit): no load, no paint.
  {
    const int before = vp.paintCount();
    write(fld, kFldEdited);
    check(nextLoad(w, 1500) == -1, "a save with the same text loads nothing");
    check(vp.paintCount() == before, "a save with the same text paints nothing");
  }

  // Write a new file and rename it over the old one, as many editors save.
  {
    const QString side = tmp.filePath(".cube.fld.new");
    write(side, kFldRenamed);
    QFile::remove(fld);
    QFile::rename(side, fld);
    check(nextLoad(w) == 1, "a write-and-rename save reloads");
    quiet(vp);
    const QImage renamedImage = vp.grabFramebuffer();
    check(renamedImage != editedImage, "the renamed file re-renders");
    write(fld, kFldEdited);
    check(nextLoad(w) == 1, "the file is still watched after the rename");
    quiet(vp);
  }

  // A broken file: the error, with its pointer, and the image stays.
  {
    const QImage before = vp.grabFramebuffer();
    QByteArray broken = gyroid;
    broken.replace("\"sphere\"", "\"no-such-op\"");
    check(drop(w, json), "drop the .json again");
    check(nextLoad(w) == 1 || vp.hasScene(), "the .json loads again");
    quiet(vp);
    const QImage good = vp.grabFramebuffer();
    check(good != before, "the .json is back on screen");
    const int paintsGood = vp.paintCount();
    write(json, broken);
    check(nextLoad(w) == 0, "a broken save fails to load");
    const QString status = w.statusBar()->currentMessage();
    std::printf("main_window_test: status: %s\n", qPrintable(status));
    check(status.contains("field-graph error at /root/"), "the status bar has the pointer");
    check(status.contains("keeping the previous scene"), "the status bar says the scene stays");
    check(w.logText().contains("field-graph error at /root/"), "the log has the pointer");
    idle(500);
    check(vp.paintCount() == paintsGood, "a broken save paints nothing");
    check(vp.grabFramebuffer() == good, "a broken save leaves the image in place");
    write(json, gyroid);
    check(nextLoad(w) == 1, "fixing the file loads it");
    quiet(vp);
    check(vp.grabFramebuffer() == good, "the fixed file renders as before");
  }

  // Undo and redo through the window (P03 T1): a hide and its undo are
  // uniforms only; an undone remove brings the object back, with its id.
  {
    // The selection is not part of the scene: a removed object loses it.
    w.selectObject(0);
    quiet(vp);
    const QImage shown = vp.grabFramebuffer();
    const int id = w.scene().objects().front().id;
    const std::size_t count = w.scene().objects().size();
    const int links = vp.linkCount();
    // Every object hidden, one command each: the background is left.
    for (const SceneObject& o : std::vector<SceneObject>(w.scene().objects()))
      w.setObjectVisible(o.id, false);
    quiet(vp);
    check(vp.grabFramebuffer() != shown, "a hide changes the image");
    for (std::size_t i = 0; i < count; ++i) w.undoStack()->undo();
    quiet(vp);
    check(w.scene().find(id)->visible, "undo shows the object again");
    check(vp.grabFramebuffer() == shown, "undo of a hide restores the image");
    w.undoStack()->redo();
    check(!w.scene().find(id)->visible, "redo hides it again");
    check(vp.linkCount() == links, "a hide, its undo and redo link nothing");
    w.undoStack()->undo();
    w.renameObject(id, "renamed");
    check(w.scene().find(id)->name == "renamed" &&
              w.objects()->row(id)->text() == "renamed",
          "a rename reaches the scene and its row");
    w.undoStack()->undo();
    const auto bindingsBefore = vp.scene().bindings;
    const std::string sourceBefore = vp.scene().generatedSource;
    w.removeObject(id);
    check(runUntil([&] { return !w.scene().find(id); }, 10000), "the remove lands");
    w.undoStack()->undo();
    check(runUntil([&] { return w.scene().find(id) != nullptr; }, 30000),
          "undo of a remove brings the object back");
    check(w.scene().objects().size() == count, "with every object");
    quiet(vp);
    check(vp.scene().generatedSource == sourceBefore, "undo of a remove: the same source");
    bool sameValues = bindingsBefore.size() == vp.scene().bindings.size();
    for (std::size_t i = 0; sameValues && i < bindingsBefore.size(); ++i)
      sameValues = bindingsBefore[i].name == vp.scene().bindings[i].name &&
                   bindingsBefore[i].value == vp.scene().bindings[i].value;
    // Not the image: across a relink, this box's NV92 driver draws the same
    // shader and uniforms differently (HANDOFF_P03_TASK1.md).
    check(sameValues, "undo of a remove: the same uniforms and values");
  }

  // The transform gizmo (P03 T3): a drag from the viewport and a value typed
  // in the Transform dock are each one undo step and uniforms only, and a
  // moved object is drawn whole: the trace box follows it, with no compile.
  {
    w.openFile(json);
    check(runUntil([&] { return w.scene().objects().size() == 1 && !vp.building(); }, 30000),
          "gizmo: one object on screen");
    quiet(vp);
    const int id = w.scene().objects().front().id;
    w.selectObject(id);
    check(vp.gizmoTarget() == id, "gizmo: the selection is the gizmo's target");
    const auto pressF = [&] {
      QKeyEvent f(QEvent::KeyPress, Qt::Key_F, Qt::NoModifier, "f");
      QApplication::sendEvent(&vp, &f);
      quiet(vp);
    };
    // Pixels the object covers: those that change when it is hidden.
    const auto covered = [&] {
      const QImage shown = vp.grabFramebuffer();
      w.setObjectVisible(id, false);
      quiet(vp);
      const QImage hidden = vp.grabFramebuffer();
      w.undoStack()->undo();
      quiet(vp);
      long n = 0;
      for (int y = 0; y < shown.height(); ++y)
        for (int x = 0; x < shown.width(); ++x) n += shown.pixel(x, y) != hidden.pixel(x, y);
      return n;
    };
    pressF();
    const long before = covered();
    const dualc::BBox box = vp.scene().bounds;
    const int links = vp.linkCount(), compiles = vp.compileCount();
    const int steps = w.undoStack()->index();
    // A drag along x, as g then x then a click make it.
    vp.startGizmo(GizmoMode::Move, QPointF(vp.width() / 2.0, vp.height() / 2.0));
    vp.setGizmoAxis(0);
    vp.moveGizmo(QPointF(vp.width() * 0.9, vp.height() / 2.0));
    vp.endGizmo(true);
    const SceneObject* o = w.document()->scene().find(id);
    check(w.undoStack()->index() == steps + 1 && w.undoStack()->undoText().startsWith("Move"),
          "gizmo: a drag is one Move step");
    check(o->transform.m[0][3] > 0.0 && o->transform.m[1][3] == 0.0 && o->transform.m[2][3] == 0.0,
          "gizmo: a drag along x moves in x only");
    // Then far out of the old box, typed in the dock: 3 box widths in x, in
    // whole mm (the box shows 3 decimals).
    const double far = std::ceil(3.0 * (box.max.x - box.min.x));
    w.transformPanel()->box(0)->setValue(far);
    o = w.document()->scene().find(id);
    check(o->transform.m[0][3] == far, "dock: the typed x is the pose's x, in mm");
    check(w.undoStack()->index() == steps + 2, "dock: a typed value is one step");
    quiet(vp);
    check(vp.scene().bounds.min.x > box.max.x, "the trace box follows the move");
    pressF();
    const long after = covered();
    check(after > 0 && std::abs(after - before) <= before / 50 + 4,
          "the moved object is drawn whole: as many pixels as before, framed");
    w.transformPanel()->box(6)->setValue(2.0);
    quiet(vp);
    check(w.document()->scene().find(id)->scale == 2.0, "dock: the scale is set");
    check(vp.scene().bounds.max.x - vp.scene().bounds.min.x > 1.9 * (box.max.x - box.min.x),
          "the trace box follows the scale");
    check(vp.linkCount() == links && vp.compileCount() == compiles,
          "a move and a scale compile and link nothing");
    // A cancelled drag puts the pose back.
    const dualc::Mat4 kept = w.document()->scene().find(id)->transform;
    vp.startGizmo(GizmoMode::Rotate, QPointF(10, 10));
    vp.moveGizmo(QPointF(200, 300));
    vp.endGizmo(false);
    check(w.undoStack()->index() == steps + 3 &&
              std::memcmp(w.document()->scene().find(id)->transform.m, kept.m, sizeof kept.m) == 0,
          "a cancelled drag leaves the pose and the history");
    for (int i = 0; i < 3; ++i) w.undoStack()->undo();
    quiet(vp);
    check(w.document()->scene().find(id)->scale == 1.0 &&
              w.document()->scene().find(id)->transform.m[0][3] == 0.0,
          "undo puts the pose back");
    check(vp.scene().bounds.min.x == box.min.x && vp.scene().bounds.max.x == box.max.x,
          "and the trace box");
  }

  // A scene saved and opened again (P03 T2): the same objects, shader and
  // uniform values, with no undo history; a drop opens it too, --snapshot
  // draws it, and a file that is no scene is refused with a message.
  {
    quiet(vp);
    QDir(tmp.path()).mkdir("saved");
    const QString path = tmp.filePath("saved/window.dcscene");
    check(w.saveScene(path) && w.scenePath() == path, "the window saves the scene");
    const Scene before = w.scene();
    const std::string source = vp.scene().generatedSource;
    const auto bindings = vp.scene().bindings;
    w.openFile(path);
    check(nextLoad(w, 30000) == 1, "the saved scene opens");
    quiet(vp);
    bool same = w.scene().objects().size() == before.objects().size();
    for (std::size_t i = 0; same && i < before.objects().size(); ++i)
      same = w.scene().objects()[i].id == before.objects()[i].id &&
             w.scene().objects()[i].name == before.objects()[i].name &&
             w.scene().objects()[i].visible == before.objects()[i].visible;
    check(same, "the opened scene has the saved objects");
    check(vp.scene().generatedSource == source, "the opened scene: the same source");
    bool sameValues = bindings.size() == vp.scene().bindings.size();
    for (std::size_t i = 0; sameValues && i < bindings.size(); ++i)
      sameValues = bindings[i].value == vp.scene().bindings[i].value;
    check(sameValues, "the opened scene: the same uniform values");
    check(!w.undoStack()->canUndo(), "opening a scene clears the undo history");
    check(drop(w, path) && nextLoad(w, 30000) == 1, "a dropped .dcscene opens");
    const QString png = tmp.filePath("scene.png");
    const int code = runEditor(editor, {"--snapshot", png, path});
    check(code == 0 || code == kExitSkip, "--snapshot draws a .dcscene");
    const QString bad = tmp.filePath("saved/bad.dcscene");
    write(bad, "not a zip");
    int result = -1;
    const QMetaObject::Connection c =
        QObject::connect(&w, &MainWindow::loadFinished, [&](bool ok) { result = ok; });
    w.openFile(bad);
    runUntil([&] { return result != -1; }, 10000);
    QObject::disconnect(c);
    check(result == 0 && w.statusBar()->currentMessage().contains("not a scene file"),
          "a file that is no scene is refused with a message");
    check(w.scene().objects().size() == before.objects().size(), "and the scene stays");
  }

  // A shader that does not link: sceneFailed, the scene and image stay.
  {
    const QImage before = vp.grabFramebuffer();
    const int nodes = vp.scene().nodeCount;
    dce::fieldgraph::GlslScene bad = vp.scene();
    bad.generatedSource += "\nthis is not glsl;\n";
    bad.nodeCount = nodes + 1;
    QString error;
    QObject::connect(&vp, &Viewport::sceneFailed, [&](const QString& e) { error = e; });
    vp.setScene(bad, true);
    check(!error.isEmpty(), "a broken shader reports its log");
    check(vp.scene().nodeCount == nodes, "a broken shader keeps the scene");
    idle(500);
    check(vp.grabFramebuffer() == before, "a broken shader leaves the image in place");
  }

  // Import as a solid (P04 T2): a closed cube and an open box dropped on a
  // fresh window give one `mesh` object and one `winding` object, the Objects
  // dock says which, and the status bar names the grid they preview at. Make
  // Watertight on the box runs on a thread while the window keeps processing
  // events, shows progress, and replaces it with a `mesh` object, one undo
  // step; cancelled at once, it changes nothing.
  {
    MainWindow m(1.0f);
    m.resize(800, 600);
    m.show();
    const QString closed = tmp.filePath("closed.obj");
    const QString open = tmp.filePath("open.obj");
    write(closed, kCubeObj);
    QByteArray box(kCubeObj);
    box.chop(QByteArray("f 4 1 5\nf 4 5 8\n").size());
    write(open, box);
    check(drop(m, closed) && nextLoad(m, 30000) == 1, "a dropped closed .obj loads");
    check(drop(m, open) && nextLoad(m, 30000) == 1, "a dropped open .obj loads");
    const auto& objs = m.scene().objects();
    check(objs.size() == 2 && objs[0].graph.op == "mesh" && objs[1].graph.op == "winding",
          "the closed mesh is a mesh object, the open one a winding object");
    check(objs.size() == 2 && m.objects()->kind(objs[0].id) == "mesh" &&
              m.objects()->kind(objs[1].id) == "winding",
          "the Objects dock shows which is which");
    check(m.timingText().contains("mesh grid 96³"),
          "the status bar names the preview grid: " + m.timingText().toStdString());
    const int soup = objs.size() == 2 ? objs[1].id : 0;
    m.selectObject(soup);
    m.setWatertightDepth(7);
    int ticks = 0, maxValue = 0;
    QTimer ticker;
    QObject::connect(&ticker, &QTimer::timeout, [&] {
      ++ticks;
      if (QProgressDialog* d = m.watertightDialog()) maxValue = std::max(maxValue, d->value());
    });
    ticker.start(1);
    int finished = -1;
    QObject::connect(&m, &MainWindow::watertightFinished, [&](bool ok) { finished = ok; });
    const int steps = m.undoStack()->count();
    check(m.makeWatertight(soup) && m.watertightRunning() && m.watertightDialog(),
          "Make Watertight starts on the winding object");
    check(!m.makeWatertight(soup), "a second job is refused while one runs");
    runUntil([&] { return finished != -1; }, 60000);
    ticker.stop();
    check(finished == 1, "Make Watertight finishes");
    std::printf("main_window_test: make watertight: %d timer ticks while it ran, progress %d/1000\n",
                ticks, maxValue);
    check(ticks > 1, "the event loop ran while it worked (the UI does not freeze)");
    check(maxValue > 0, "the dialog showed progress");
    check(nextLoad(m, 30000) != 0 && m.scene().find(soup) &&
              m.scene().find(soup)->graph.op == "mesh" && m.objects()->kind(soup) == "mesh",
          "the winding object became a mesh object");
    check(m.undoStack()->count() == steps + 1 &&
              m.undoStack()->undoText().startsWith("Make watertight"),
          "it is one undo step: " + m.undoStack()->undoText().toStdString());
    m.undoStack()->undo();
    runUntil([&] { return !m.viewport()->building() && m.scene().find(soup) &&
                          m.scene().find(soup)->graph.op == "winding"; }, 30000);
    check(m.scene().find(soup) && m.scene().find(soup)->graph.op == "winding",
          "undo brings the winding object back");
    finished = -1;
    m.selectObject(soup);
    check(m.makeWatertight(soup), "Make Watertight starts again");
    // The dialog's Cancel button, as a click would press it.
    if (QPushButton* cancel = m.watertightDialog()->findChild<QPushButton*>()) cancel->click();
    runUntil([&] { return finished != -1; }, 60000);
    check(finished == 0 && m.scene().find(soup)->graph.op == "winding" &&
              m.undoStack()->count() == steps + 1 &&
              m.statusBar()->currentMessage().contains("cancelled"),
          "cancelled, it leaves the object and the history alone: " +
              m.statusBar()->currentMessage().toStdString());
  }

  // Booleans (P05 T1): two objects Ctrl+clicked in the Objects dock, the
  // winding one first, give a smooth union in that order with the menu's k, the
  // winding operand under normalize (E-10), which the step and the log name.
  // The result is selected; undo shows the operands again.
  {
    MainWindow m(1.0f);
    m.resize(800, 600);
    m.show();
    check(drop(m, tmp.filePath("closed.obj")) && nextLoad(m, 30000) == 1 &&
              drop(m, tmp.filePath("open.obj")) && nextLoad(m, 30000) == 1,
          "booleans: a closed and an open mesh load");
    const std::vector<SceneObject> objs = m.scene().objects();
    const int cube = objs.size() == 2 ? objs[0].id : 0, soup = objs.size() == 2 ? objs[1].id : 0;
    QAction* smoothUnion = nullptr;
    for (QAction* a : m.findChildren<QAction*>())
      if (a->data().toString() == "smooth-union") smoothUnion = a;
    m.objects()->selectInOrder({soup});
    check(smoothUnion && !smoothUnion->isEnabled() && m.applyBoolean("union") == 0,
          "one object selected: the booleans are off");
    m.objects()->selectInOrder({soup, cube});
    check(smoothUnion && smoothUnion->isEnabled() &&
              m.objects()->selectedIds() == std::vector<int>{soup, cube},
          "two objects selected, in selection order: the booleans are on");
    m.setSmoothK(0.5);
    if (smoothUnion) smoothUnion->trigger();
    check(runUntil([&] { return m.scene().objects().size() == 3; }, 30000),
          "the smooth union is on screen");
    if (m.scene().objects().size() == 3) {
      const SceneObject r = m.scene().objects().back();
      check(r.graph.op == "smooth-union" && r.graph.params.count("k") &&
                r.graph.params.at("k").num == 0.5 && r.graph.in.size() == 2 &&
                r.graph.in[0].op == "normalize" && r.graph.in[0].in[0].op == "winding" &&
                r.graph.in[1].op == "mesh",
            "smooth-union(normalize(winding), mesh), k = 0.5 mm, in selection order");
      check(r.operands == std::vector<int>{soup, cube}, "its operands, in order");
      check(m.selectedObject() == r.id, "the new object is selected");
      check(!m.scene().find(cube)->visible && !m.scene().find(soup)->visible,
            "the operands are hidden");
      check(m.undoStack()->undoText().contains("(normalize on open.obj)"),
            "the step names the normalize: " + m.undoStack()->undoText().toStdString());
      check(m.logText().contains("normalize on open.obj, not distance fields (E-10)"),
            "the log names it too");
      m.undoStack()->undo();
      check(runUntil([&] { return m.scene().objects().size() == 2; }, 30000) &&
                m.scene().find(cube)->visible && m.scene().find(soup)->visible,
            "undo removes it and shows the operands again");
    }
  }

  // The command-line argument, through --snapshot.
  {
    const QString png = tmp.filePath("cube.png");
    const int code = runEditor(editor, {"--snapshot", png, fld});
    if (code == kExitSkip) {
      std::printf("main_window_test: snapshot skipped, no GL 3.3 context\n");
    } else {
      QImage image(png);
      check(code == 0 && image.width() == 800, "--snapshot renders a .fld argument");
    }
    QByteArray broken = gyroid;
    broken.replace("\"sphere\"", "\"no-such-op\"");
    const QString bad = tmp.filePath("broken.json");
    write(bad, broken);
    check(runEditor(editor, {"--snapshot", png, bad}) == 2,
          "--snapshot of a broken argument exits 2");
    check(runEditor(editor, {"--snapshot", png, tmp.filePath("missing.fld")}) == 2,
          "--snapshot of a missing file exits 2");
    // The editor runs in the test's working directory, not beside the graph.
    QDir(tmp.path()).mkdir("meshes");
    write(tmp.filePath("meshes/cube.obj"), kCubeObj);
    write(tmp.filePath("meshes/cut.fld"), kMeshFld);
    const int meshCode = runEditor(editor, {"--snapshot", png, tmp.filePath("meshes/cut.fld")});
    check(meshCode == 0 || meshCode == kExitSkip,
          "a relative mesh path resolves beside the graph");
  }

  if (failures) {
    std::fprintf(stderr, "main_window_test: %d check(s) failed\n", failures);
    return 1;
  }
  std::printf("main_window_test: all checks passed\n");
  return 0;
}
