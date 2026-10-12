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
#include "viewport.h"

#include <QApplication>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMimeData>
#include <QProcess>
#include <QSet>
#include <QStatusBar>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QUrl>
#include <QWindow>

#include <cstdio>
#include <cstdlib>

namespace {

constexpr int kExitSkip = 77;
int failures = 0;

void check(bool ok, const char* what) {
  std::fprintf(ok ? stdout : stderr, "main_window_test: %s %s\n", ok ? "ok  " : "FAIL",
               what);
  if (!ok) ++failures;
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
