// The asynchronous compile (P07 T2, E-21), on a live Viewport and MainWindow:
//  1. a build swaps the program while frames are drawn: every frame before the
//     swap is the old scene's image, every frame after it the new scene's, so
//     no frame draws one scene's program with the other's values; the GUI
//     thread runs its timers while the build runs;
//  2. of two builds in flight only the last is taken;
//  3. a build that fails to link, or whose codegen throws, keeps the scene and
//     its image, and reports;
//  4. in the window, adding an object while another add is still compiling
//     gives the scene of the last edit (both objects), each attempt ends in
//     loadFinished, a hide made during the compile survives the swap, and the
//     status bar shows "compile …" while it compiles.
// Needs a display and an OpenGL 3.3 core context; exits 77 (ctest: skipped)
// without them.
//
// usage: async_compile_test <gyroid_box.json>

#include "graph_file.h"
#include "graph_scene.h"
#include "main_window.h"
#include "scene.h"
#include "viewport.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <cstdio>
#include <stdexcept>

namespace fg = dce::fieldgraph;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  std::fprintf(ok ? stdout : stderr, "async_compile_test: %s %s\n", ok ? "ok  " : "FAIL",
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

fg::GlslScene lowered(const char* text) {
  Scene scene;
  scene.add("graph", parseGraphText(text, false));
  return lowerScene(scene).scene;
}

void write(const QString& path, const QByteArray& text) {
  QFile f(path);
  if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(text);
}

}  // namespace

int main(int argc, char** argv) {
  QSurfaceFormat fmt;
  fmt.setVersion(3, 3);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(fmt);
  QApplication app(argc, argv);
  if (argc < 2) {
    std::fprintf(stderr, "usage: async_compile_test <gyroid_box.json>\n");
    return 2;
  }

  Viewport vp(1.0f);
  vp.resize(320, 240);
  vp.show();
  runUntil([&] { return vp.isGLReady() || vp.paintCount() > 0; }, 5000);
  if (!vp.isGLReady()) {
    std::printf("async_compile_test: skipped, no OpenGL 3.3 core context\n");
    return 77;
  }
  int applied = 0;
  QString failed;
  QObject::connect(&vp, &Viewport::sceneApplied, [&] { ++applied; });
  QObject::connect(&vp, &Viewport::sceneFailed, [&](const QString& e) { failed = e; });

  // The two scenes, each drawn once as it is linked on the GUI thread.
  const GraphLoad gyroid = loadGraphFile(argv[1]);
  if (!gyroid.scene) return std::fprintf(stderr, "%s\n", qPrintable(gyroid.error)), 1;
  const fg::GlslScene a = *gyroid.scene;
  const fg::GlslScene b = lowered("difference(box(min=[-1,-1,-1],max=[1,1,1]),sphere(radius=1.25))");
  vp.setScene(b, true);
  const QImage imageB = vp.grabFramebuffer();
  vp.setScene(a, false);
  const QImage imageA = vp.grabFramebuffer();
  check(imageA != imageB, "the two scenes draw different images");

  // 1. The swap, with frames drawn all through the build.
  {
    applied = 0;
    int ticks = 0, frames = 0, wrong = 0;
    QTimer tick;
    QObject::connect(&tick, &QTimer::timeout, [&] { ++ticks; });
    tick.start(5);
    const int links = vp.linkCount();
    vp.buildScene(
        [b] {
          QThread::msleep(400);  // a slow codegen: the GUI thread must not wait
          return b;
        },
        false);
    check(vp.building(), "a build is in flight");
    while (applied == 0 && runUntil([] { return false; }, 10) == false) {
      if (applied) break;
      if (vp.grabFramebuffer() != imageA) ++wrong;
      ++frames;
      if (frames > 2000) break;
    }
    runUntil([&] { return applied > 0; }, 20000);
    tick.stop();
    std::printf("async_compile_test: %d frames and %d timer ticks during the build\n", frames,
                ticks);
    check(frames >= 2 && ticks >= 5, "the GUI thread draws and runs timers during the build");
    check(wrong == 0, "every frame before the swap is the old scene's image");
    check(applied == 1 && !vp.building(), "the build is taken once");
    check(vp.grabFramebuffer() == imageB, "every frame after the swap is the new scene's image");
    check(vp.linkCount() == links + 1 && vp.compileMs() > 0.0,
          "the build's link is counted and timed");
  }

  // 2. Two builds in flight: the last is taken.
  {
    applied = 0;
    vp.setScene(a, false);
    applied = 0;
    vp.buildScene(
        [b] {
          QThread::msleep(300);
          return b;
        },
        false);
    vp.buildScene([a] { return a; }, false);
    runUntil([&] { return !vp.building(); }, 20000);
    runUntil([] { return false; }, 600);  // a stale result would come back by now
    check(applied == 1, "of two builds in flight, one is taken");
    check(vp.grabFramebuffer() == imageA, "the last build's scene is on screen");
  }

  // 3. Failures keep the scene and the image.
  {
    applied = 0;
    failed.clear();
    fg::GlslScene bad = a;
    bad.generatedSource += "\nthis is not glsl;\n";
    vp.buildScene([bad] { return bad; }, false);
    runUntil([&] { return !failed.isEmpty(); }, 20000);
    check(!failed.isEmpty() && applied == 0, "a link failure reports its log");
    check(vp.grabFramebuffer() == imageA, "a link failure keeps the image");
    failed.clear();
    vp.buildScene([]() -> fg::GlslScene { throw std::runtime_error("no codegen"); }, false);
    runUntil([&] { return !failed.isEmpty(); }, 20000);
    check(failed == "no codegen" && vp.grabFramebuffer() == imageA,
          "a codegen error reports its text and keeps the image");
  }

  // 4. The window: an add while another add compiles.
  {
    QTemporaryDir tmp;
    const QString fa = tmp.filePath("a.fld"), fb = tmp.filePath("b.fld");
    write(fa, "translate(by=[-1.2,0,0],sphere(radius=0.8))\n");
    write(fb, "translate(by=[1.2,0,0],box(min=[-0.6,-0.6,-0.6],max=[0.6,0.6,0.6]))\n");
    MainWindow w(1.0f);
    w.resize(640, 480);
    w.show();
    Viewport& wv = *w.viewport();
    runUntil([&] { return wv.isGLReady(); }, 5000);
    int ok = 0, finished = 0;
    QObject::connect(&w, &MainWindow::loadFinished, [&](bool good) {
      ++finished;
      ok += good;
    });
    w.addFile(fa);
    runUntil([&] { return finished == 1; }, 20000);
    const int first = w.scene().objects().front().id;
    w.addFile(fb);
    check(wv.building() && w.timingText().startsWith("compile …"),
          "the status bar shows compile … while a scene compiles");
    w.setObjectVisible(first, false);  // during the compile
    w.addFile(fa);                     // a third object, while b still compiles
    runUntil([&] { return finished == 3; }, 20000);
    runUntil([] { return false; }, 300);
    check(finished == 3 && ok == 3, "each add ends in one successful loadFinished");
    check(w.scene().objects().size() == 3 && w.lowered().objects.size() == 3 &&
              wv.scene().bindings.size() == w.lowered().scene.bindings.size(),
          "the scene on screen is the last edit's: three objects");
    const LoweredObject& lo = w.lowered().objects.front();
    check(wv.scene().bindings[std::size_t(lo.visibleBinding)].value ==
                  std::vector<float>{0.0f} &&
              !w.scene().objects().front().visible,
          "a hide made while the scene compiled survives the swap");
    check(!w.timingText().contains("…"), "the compile ms is back once the scene is on screen");
  }

  std::printf("async_compile_test: %s\n", failures ? "FAILED" : "passed");
  return failures ? 1 : 0;
}
