// Drives a live Viewport with synthetic mouse, wheel and key events and checks
// the adaptive preview: frames traced at the preview scale while the camera
// moves, one full-resolution frame kSettleMs after the last input, and no
// paintGL at all while idle. Also checks that F frames by the character it
// types, not by key position. Prints the frame times it saw.
//
// Needs a display and an OpenGL 3.3 core context; exits 77 (ctest: skipped)
// without them.
//
// usage: viewport_test <graph.json> [preview-scale] [width height]

#include "graph_scene.h"
#include "viewport.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSurfaceFormat>
#include <QWheelEvent>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <vector>

namespace fg = dce::fieldgraph;

namespace {

constexpr int kExitSkip = 77;
int failures = 0;

void check(bool ok, const char* what) {
  std::fprintf(ok ? stdout : stderr, "viewport_test: %s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}

struct Frame {
  bool preview;
  QSize traced;
  double ms;
  qint64 at;  // ms on the test clock
};

QElapsedTimer testClock;
std::vector<Frame> frames;

// Run the event loop until `done` or `timeoutMs`.
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

void mouse(Viewport& vp, QEvent::Type type, QPointF pos, Qt::MouseButton button,
           Qt::MouseButtons held, Qt::KeyboardModifiers mods = Qt::NoModifier) {
  QMouseEvent e(type, pos, vp.mapToGlobal(pos), button, held, mods);
  QCoreApplication::sendEvent(&vp, &e);
}

// A drag of `steps` moves of (dx, dy), each one painted before the next.
void drag(Viewport& vp, Qt::MouseButton button, Qt::KeyboardModifiers mods, double dx,
          double dy, int steps) {
  QPointF p(vp.width() / 2.0, vp.height() / 2.0);
  mouse(vp, QEvent::MouseButtonPress, p, button, button, mods);
  for (int i = 0; i < steps; ++i) {
    p += QPointF(dx, dy);
    const int before = vp.paintCount();
    mouse(vp, QEvent::MouseMove, p, Qt::NoButton, button, mods);
    runUntil([&] { return vp.paintCount() > before; }, 10000);
  }
}

void release(Viewport& vp, Qt::MouseButton button) {
  mouse(vp, QEvent::MouseButtonRelease, QPointF(vp.width() / 2.0, vp.height() / 2.0),
        button, Qt::NoButton);
}

// Wait for the full-resolution frame that ends a settle; returns its time.
qint64 settled() {
  const std::size_t from = frames.size();
  const bool ok = runUntil(
      [&] {
        for (std::size_t i = from; i < frames.size(); ++i)
          if (!frames[i].preview) return true;
        return false;
      },
      10000);
  if (!ok) return -1;
  for (std::size_t i = from; i < frames.size(); ++i)
    if (!frames[i].preview) return frames[i].at;
  return -1;
}

// Wait until no paint has come for 500 ms (the settle frame included).
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

double median(std::vector<double> v) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[v.size() / 2];
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 2 && argc != 3 && argc != 5) {
    std::fprintf(stderr, "usage: viewport_test <graph.json> [preview-scale] [w h]\n");
    return 1;
  }
#ifdef Q_OS_LINUX
  if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
    std::printf("viewport_test: no display, skipped\n");
    return kExitSkip;
  }
#endif
  QSurfaceFormat format;
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(format);
  QApplication app(argc, argv);
  if (QGuiApplication::platformName() == "offscreen" ||
      QGuiApplication::platformName() == "minimal") {
    std::printf("viewport_test: platform %s has no window GL, skipped\n",
                qPrintable(QGuiApplication::platformName()));
    return kExitSkip;
  }

  QFile file(QString::fromLocal8Bit(argv[1]));
  if (!file.open(QIODevice::ReadOnly)) {
    std::fprintf(stderr, "viewport_test: cannot read %s\n", argv[1]);
    return 1;
  }
  fg::FileMeshResolver meshes;
  fg::GlslScene scene = compileGraphText(file.readAll().toStdString(), true, meshes);
  const float scale = argc >= 3 ? float(std::atof(argv[2])) : 0.25f;
  const int width = argc == 5 ? std::atoi(argv[3]) : 800;
  const int height = argc == 5 ? std::atoi(argv[4]) : 600;

  Viewport vp(std::move(scene), scale);
  vp.setFrameTiming(true);
  vp.resize(width, height);
  testClock.start();
  QObject::connect(&vp, &Viewport::frameDrawn, [](bool preview, QSize traced, double ms) {
    frames.push_back({preview, traced, ms, testClock.elapsed()});
  });
  vp.show();

  // Opening: a preview frame first, then the settled full-resolution one.
  // The window manager may resize the window once it is mapped, which
  // repaints, so wait for the window to be exposed and the paints to stop.
  const bool exposed = runUntil(
      [&] { return vp.windowHandle() && vp.windowHandle()->isExposed(); }, 10000);
  if (!exposed || !runUntil([&] { return !frames.empty(); }, 10000)) {
    if (!vp.isGLReady()) {
      std::printf("viewport_test: no OpenGL 3.3 core context, skipped\n");
      return kExitSkip;
    }
    check(false, "the viewport paints after opening");
    return 1;
  }
  quiet(vp);
  const bool previewOn = vp.previewScale() < 0.999f;
  const qreal dpr = vp.devicePixelRatioF();
  const QSize full(qRound(vp.width() * dpr), qRound(vp.height() * dpr));
  const QSize low(std::max(1, int(float(full.width()) * vp.previewScale())),
                  std::max(1, int(float(full.height()) * vp.previewScale())));
  const QSize moving = previewOn ? low : full;
  check(frames.front().preview == previewOn,
        previewOn ? "the first frame is a preview" : "the first frame is full (preview off)");
  check(!frames.back().preview && frames.back().traced == full,
        "the opening settles on a full-resolution frame");

  // Idle: nothing repaints.
  int before = vp.paintCount();
  idle(1000);
  check(vp.paintCount() == before, "no paintGL in 1 s of idle after opening");

  // Left-drag orbits at preview scale; holding the button keeps the preview.
  const OrbitCamera cam0 = vp.camera();
  std::size_t from = frames.size();
  drag(vp, Qt::LeftButton, Qt::NoModifier, 10, 0, 20);
  check(std::abs(vp.camera().yaw - (cam0.yaw - 200 * 0.008)) < 1e-9,
        "left-drag of 200 px orbits yaw by -1.6 rad");
  bool allMoving = frames.size() == from + 20;
  std::vector<double> movingMs;
  for (std::size_t i = from; i < frames.size(); ++i) {
    allMoving = allMoving && frames[i].preview == previewOn && frames[i].traced == moving;
    movingMs.push_back(frames[i].ms);
  }
  check(allMoving, "one frame per drag move, traced at the preview size");
  before = vp.paintCount();
  idle(2 * Viewport::kSettleMs);
  check(vp.paintCount() == before, "a held, still drag repaints nothing");

  // Release: with the preview on, one full frame after the settle time; with
  // it off, nothing (the last frame already is full). Then nothing.
  const qint64 releasedAt = testClock.elapsed();
  before = vp.paintCount();
  release(vp, Qt::LeftButton);
  if (previewOn) {
    const qint64 settledAt = settled();
    check(settledAt >= releasedAt + Viewport::kSettleMs - 5,
          "the full frame comes no sooner than the settle time after release");
    check(settledAt >= 0 && settledAt <= releasedAt + Viewport::kSettleMs + 1000,
          "the full frame comes within 1 s of the settle time");
    check(vp.paintCount() == before + 1 && frames.back().traced == full,
          "the release settles with exactly one full-resolution frame");
    before = vp.paintCount();
  }
  idle(1000);
  check(vp.paintCount() == before, "no paintGL in 1 s of idle after a drag");

  // Wheel: one notch away from the user dollies in by 0.9.
  const double dist0 = vp.camera().dist;
  {
    const QPointF c(width / 2.0, height / 2.0);
    QWheelEvent e(c, vp.mapToGlobal(c), QPoint(), QPoint(0, 120), Qt::NoButton,
                  Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&vp, &e);
  }
  check(std::abs(vp.camera().dist - dist0 * 0.9) < 1e-9, "one wheel notch dollies by 0.9");
  if (previewOn) check(settled() >= 0, "the wheel settles to full resolution");

  // Shift+left-drag and middle-drag pan: the target moves, the angles do not.
  for (const auto& [button, mods, what] :
       {std::tuple{Qt::LeftButton, Qt::KeyboardModifiers(Qt::ShiftModifier),
                   "shift+left-drag pans"},
        std::tuple{Qt::MiddleButton, Qt::KeyboardModifiers(Qt::NoModifier),
                   "middle-drag pans"}}) {
    const OrbitCamera c = vp.camera();
    drag(vp, button, mods, 5, 5, 4);
    release(vp, button);
    const dualc::Vector3 moved = vp.camera().target - c.target;
    check(moved.norm() > 1e-6 && vp.camera().yaw == c.yaw && vp.camera().pitch == c.pitch,
          what);
    idle(2 * Viewport::kSettleMs);
  }

  // F frames the bounds. Matched by the typed character: a key event that
  // types "f" frames whatever its key code or scan code (a layout where the
  // F key sits elsewhere), and one at F's QWERTY position typing another
  // character does not.
  auto key = [&](int code, quint32 scan, const QString& text) {
    QKeyEvent e(QEvent::KeyPress, code, Qt::NoModifier, scan, 0, 0, text);
    QCoreApplication::sendEvent(&vp, &e);
  };
  const OrbitCamera panned = vp.camera();
  key(Qt::Key_D, 41, "d");  // X11 scan code 41 is F's position on QWERTY
  check(vp.camera().target == panned.target, "a key typing 'd' at F's position does not frame");
  key(Qt::Key_unknown, 30, "f");
  check(vp.camera().dist == vp.camera().dist0 &&
            (vp.camera().target - panned.target).norm() > 1e-6,
        "a key typing 'f' frames the bounds, whatever its position");
  const dualc::Vector3 framed = vp.camera().target;
  idle(2 * Viewport::kSettleMs);
  drag(vp, Qt::MiddleButton, Qt::NoModifier, 5, 5, 4);
  release(vp, Qt::MiddleButton);
  idle(2 * Viewport::kSettleMs);
  key(Qt::Key_F, 41, "F");
  check((vp.camera().target - framed).norm() < 1e-12, "shift+F frames too");
  idle(2 * Viewport::kSettleMs);

  std::vector<double> fullMs;
  for (const Frame& f : frames)
    if (!f.preview) fullMs.push_back(f.ms);
  std::printf(
      "viewport_test: %dx%d (dpr %.2f), preview %.2f: drag frame median %.1f ms at "
      "%dx%d, full frame median %.1f ms at %dx%d, %d paints\n",
      width, height, dpr, double(vp.previewScale()), median(movingMs), low.width(),
      low.height(), median(fullMs), full.width(), full.height(), vp.paintCount());
  return failures == 0 ? 0 : 1;
}
