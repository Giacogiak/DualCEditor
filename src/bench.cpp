#include "bench.h"

#include "snapshot.h"
#include "viewport.h"

#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLContext>
#include <QScreen>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace {

// Frames dropped at the start of each phase, while the GPU clocks up and the
// preview target is first allocated.
constexpr int kWarmFrames = 3;
// A phase that has not finished after this long fails the run.
constexpr int kPhaseLimitMs = 60000;
// One orbit step per moving frame, in drag pixels: about a quarter turn per
// 200 frames, so the surface under each pixel keeps changing.
constexpr double kOrbitStepPx = 1.0;

double median(std::vector<double> v) {
  if (v.empty()) return std::nan("");
  std::sort(v.begin(), v.end());
  const std::size_t n = v.size();
  return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

QJsonValue number(double ms) {
  return std::isnan(ms) ? QJsonValue() : QJsonValue(std::round(ms * 100.0) / 100.0);
}

QJsonArray sizeJson(QSize s) { return {s.width(), s.height()}; }

struct Phase {
  std::vector<double> gpuMs, wallMs;
  QSize traced;
  QJsonObject json() const {
    return {{"gpuMs", number(median(gpuMs))},
            {"wallMs", number(median(wallMs))},
            {"gpuFrames", int(gpuMs.size())},
            {"wallFrames", int(wallMs.size())},
            {"traced", sizeJson(traced)}};
  }
};

}  // namespace

int runBench(dce::fieldgraph::GlslScene scene, const BenchOptions& options) {
  // No display (the offscreen platform) or no GL 3.3 core: nothing to time.
  {
    QOpenGLContext probe;
    if (!probe.create() || probe.format().version() < qMakePair(3, 3)) {
      std::fprintf(stderr, "DualCEditor: cannot create an OpenGL 3.3 core context\n");
      return kExitNoGl;
    }
  }
  Viewport view(options.previewScale);
  view.setWindowTitle("DualCEditor bench");
  // The traced size is the widget size in device pixels.
  const qreal dpr = QGuiApplication::primaryScreen()
                        ? QGuiApplication::primaryScreen()->devicePixelRatio()
                        : 1.0;
  view.setFixedSize(qRound(options.size.width() / dpr), qRound(options.size.height() / dpr));

  bool failed = false;
  QString error;
  QEventLoop loop;
  // A failed link, or no GL context, comes when GL first comes up.
  QObject::connect(&view, &Viewport::sceneFailed, &loop, [&](const QString& e) {
    failed = true;
    error = e;
    loop.quit();
  });
  QElapsedTimer sinceStart;  // to the first presented frame: link and first trace
  sinceStart.start();
  double firstFrameMs = std::nan("");
  view.setScene(std::move(scene), true);
  view.show();

  enum class Step { Warm, Moving, Draining, Settled, Done };
  Step step = Step::Warm;
  Phase moving, settled;
  int presented = 0;  // frames presented in the current phase
  int timed = 0;      // GPU times received in the current phase
  QElapsedTimer sinceSwap;
  QTimer limit;
  limit.setSingleShot(true);
  limit.setInterval(kPhaseLimitMs);
  QObject::connect(&limit, &QTimer::timeout, &loop, [&] {
    failed = true;
    error = "the run stalled";
    loop.quit();
  });

  const bool previewing = view.previewScale() < 0.999f;
  QObject::connect(&view, &Viewport::frameDrawn, [&](bool, QSize traced, double) {
    if (step == Step::Moving) moving.traced = traced;
    if (step == Step::Settled) settled.traced = traced;
  });
  // GPU times arrive a few ms after their frame. The drain between the phases
  // is longer than that, so a time goes to the phase it arrives in. With the
  // preview on, the settle frame drawn while draining is not a moving frame.
  QObject::connect(&view, &Viewport::gpuFrameTimed, [&](bool preview, QSize, double ms) {
    if ((step == Step::Moving || step == Step::Draining) && preview == previewing) {
      if (++timed > kWarmFrames) moving.gpuMs.push_back(ms);
    } else if ((step == Step::Settled || step == Step::Done) && !preview) {
      if (++timed > kWarmFrames) settled.gpuMs.push_back(ms);
      if (step == Step::Done && timed >= options.frames + kWarmFrames) loop.quit();
    }
  });

  auto startPhase = [&](Step next) {
    step = next;
    presented = 0;
    timed = 0;
    limit.start();
  };
  QObject::connect(&view, &QOpenGLWidget::frameSwapped, [&] {
    const double wall = sinceSwap.isValid() ? sinceSwap.nsecsElapsed() * 1e-6 : 0.0;
    sinceSwap.restart();
    switch (step) {
      case Step::Warm:
        // Let the opening preview settle to a full frame first.
        if (presented == 0) firstFrameMs = sinceStart.nsecsElapsed() * 1e-6;
        if (presented++ == 0)
          QTimer::singleShot(Viewport::kSettleMs * 3, &view, [&] {
            startPhase(Step::Moving);
            view.orbitByPixels(kOrbitStepPx, 0.0);
          });
        break;
      case Step::Moving:
        if (++presented > kWarmFrames) moving.wallMs.push_back(wall);
        if (presented < options.frames + kWarmFrames) {
          view.orbitByPixels(kOrbitStepPx, 0.0);
        } else {
          // Stop moving; the view draws its settle frame on its own.
          step = Step::Draining;
          QTimer::singleShot(Viewport::kSettleMs * 3, &view, [&] {
            startPhase(Step::Settled);
            view.update();
          });
        }
        break;
      case Step::Draining:
        break;
      case Step::Settled:
        if (++presented > kWarmFrames) settled.wallMs.push_back(wall);
        if (presented < options.frames + kWarmFrames) {
          view.update();
        } else {
          step = Step::Done;
          // The last GPU times come back within a few ms; a frame drawn while
          // every query was in flight never does.
          if (timed >= options.frames + kWarmFrames) loop.quit();
          QTimer::singleShot(1000, &loop, &QEventLoop::quit);
        }
        break;
      case Step::Done:
        break;
    }
  });

  limit.start();
  if (!failed) loop.exec();

  if (!view.isGLReady()) {
    std::fprintf(stderr, "DualCEditor: cannot create an OpenGL 3.3 core context\n");
    return kExitNoGl;
  }
  if (failed) {
    std::fprintf(stderr, "DualCEditor: %s\n", qPrintable(error));
    return 1;
  }
  QJsonObject out{{"graph", options.graph},
                  {"previewScale", double(view.previewScale())},
                  {"frames", options.frames},
                  {"compileMs", number(view.compileMs())},
                  {"firstFrameMs", number(firstFrameMs)},
                  {"moving", moving.json()},
                  {"settled", settled.json()},
                  {"glRenderer", view.glRenderer()},
                  {"glVersion", view.glVersion()},
                  {"qt", QString::fromLatin1(qVersion())},
                  {"version", QCoreApplication::applicationVersion()}};
  std::printf("%s\n", QJsonDocument(out).toJson(QJsonDocument::Compact).constData());
  std::fflush(stdout);
  return 0;
}
