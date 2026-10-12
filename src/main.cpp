#include "gpu_preference.h"  // Optimus/PowerXpress opt-in (Windows); one TU only
#include "bench.h"
#include "graph_file.h"
#include "main_window.h"
#include "scene.h"
#include "snapshot.h"

#include <QApplication>
#include <QFileInfo>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QStringList>
#include <QSurfaceFormat>

#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

namespace {

// The graph the window opens on when no file is named: DualC's
// examples/samples/gyroid_box.json, embedded at build time.
constexpr const char* kDefaultGraph = ":/samples/gyroid_box.json";

bool hasFlag(int argc, char* argv[], const char* flag) {
  const std::size_t n = std::strlen(flag);
  for (int i = 1; i < argc; ++i)
    if (std::strncmp(argv[i], flag, n) == 0 && (argv[i][n] == '\0' || argv[i][n] == '='))
      return true;
  return false;
}

// The graphs as a scene of one object each, lowered to one shader. On an
// error it prints the reason and returns false.
bool lowerGraphs(const QStringList& paths, dce::fieldgraph::GlslScene* out) {
  Scene scene;
  for (const QString& path : paths) {
    GraphParse parse = parseGraphFile(path);
    if (!parse.graph) {
      std::fprintf(stderr, "DualCEditor: %s\n", qPrintable(parse.error));
      return false;
    }
    scene.add(QFileInfo(path).fileName(), std::move(*parse.graph), graphMeshDir(path));
  }
  try {
    *out = lowerScene(scene).scene;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "DualCEditor: field-graph error: %s\n", e.what());
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  // The sphere-tracer DualC generates is GLSL 330 core; ask for that context
  // before any widget or surface exists.
  QSurfaceFormat format;
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  // A measurement is not capped at the display's refresh rate: frames are
  // presented as fast as they are traced, as with vsync off in the driver.
  if (hasFlag(argc, argv, "--bench")) format.setSwapInterval(0);
  QSurfaceFormat::setDefaultFormat(format);

  // A snapshot opens no window, so it needs no widgets, and on a Linux box
  // with no display it falls back to the offscreen platform rather than
  // aborting: there it reports "no GL context" and the ctest skips.
  const bool snapshot = hasFlag(argc, argv, "--snapshot");
#ifdef Q_OS_LINUX
  // --bench needs a window, so it reports "no GL context" there too.
  if ((snapshot || hasFlag(argc, argv, "--bench")) && qEnvironmentVariableIsEmpty("DISPLAY") &&
      qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") &&
      qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
  std::unique_ptr<QGuiApplication> app =
      snapshot ? std::make_unique<QGuiApplication>(argc, argv)
               : std::make_unique<QApplication>(argc, argv);
  QCoreApplication::setApplicationName("DualCEditor");
  QCoreApplication::setApplicationVersion(DUALCEDITOR_VERSION);

  QCommandLineParser parser;
  parser.setApplicationDescription("Live editor for DualC field graphs.");
  parser.addHelpOption();
  parser.addVersionOption();
  const QCommandLineOption snapshotOption(
      "snapshot", "Render one 800x600 frame off-screen to the PNG <path> and exit.",
      "path");
  parser.addOption(snapshotOption);
  const QCommandLineOption previewOption(
      "preview-scale",
      "Resolution factor while the view moves, 0.1 to 1 (1 turns the preview off). "
      "Default 0.5.",
      "S", "0.5");
  parser.addOption(previewOption);
  const QCommandLineOption benchOption(
      "bench",
      "Time the view of [graph] while orbiting and when settled, print one JSON line "
      "and exit (docs/command_reference/01-viewport.md).");
  parser.addOption(benchOption);
  const QCommandLineOption benchSizeOption(
      "bench-size", "Traced size of --bench, in device pixels. Default 1280x800.", "WxH",
      "1280x800");
  parser.addOption(benchSizeOption);
  const QCommandLineOption benchFramesOption(
      "bench-frames", "Frames timed per --bench phase. Default 60.", "N", "60");
  parser.addOption(benchFramesOption);
  parser.addPositionalArgument(
      "graph",
      "Field graph to open (.json or .fld shorthand). Default: gyroid_box.json. "
      "Several make a scene of one object per graph.",
      "[graph...]");
  parser.process(*app);
  const QStringList graphs = parser.positionalArguments();
  const QString graph = parser.positionalArguments().isEmpty()
                            ? QString(kDefaultGraph)
                            : parser.positionalArguments().first();

  bool scaleOk = false;
  const float previewScale = parser.value(previewOption).toFloat(&scaleOk);
  if (!scaleOk) {
    std::fprintf(stderr, "DualCEditor: --preview-scale needs a number, got '%s'\n",
                 qPrintable(parser.value(previewOption)));
    return 1;
  }

  if (parser.isSet(snapshotOption) && graphs.size() > 1) {
    dce::fieldgraph::GlslScene scene;
    if (!lowerGraphs(graphs, &scene)) return 2;
    return renderSnapshot(scene, parser.value(snapshotOption), 800, 600);
  }
  if (parser.isSet(snapshotOption)) {
    GraphLoad load = loadGraphFile(graph);
    if (!load.scene) {
      std::fprintf(stderr, "DualCEditor: %s\n", qPrintable(load.error));
      return 2;
    }
    return renderSnapshot(*load.scene, parser.value(snapshotOption), 800, 600);
  }

  if (parser.isSet(benchOption)) {
    BenchOptions bench;
    bench.graph = graphs.size() > 1 ? graphs.join(' ') : graph;
    bench.previewScale = previewScale;
    const QStringList wh = parser.value(benchSizeOption).split('x');
    bool wOk = false, hOk = false, nOk = false;
    if (wh.size() == 2) bench.size = QSize(wh[0].toInt(&wOk), wh[1].toInt(&hOk));
    bench.frames = parser.value(benchFramesOption).toInt(&nOk);
    if (!wOk || !hOk || bench.size.width() < 16 || bench.size.height() < 16 || !nOk ||
        bench.frames < 1) {
      std::fprintf(stderr, "DualCEditor: --bench-size needs WxH and --bench-frames N >= 1\n");
      return 1;
    }
    if (graphs.size() > 1) {
      dce::fieldgraph::GlslScene scene;
      if (!lowerGraphs(graphs, &scene)) return 2;
      return runBench(std::move(scene), bench);
    }
    GraphLoad load = loadGraphFile(graph);
    if (!load.scene) {
      std::fprintf(stderr, "DualCEditor: %s\n", qPrintable(load.error));
      return 2;
    }
    return runBench(std::move(*load.scene), bench);
  }

  // A graph that does not load still opens the window: the error is in the
  // status bar and the log, and the file is watched, so fixing it renders it.
  MainWindow window(previewScale);
  window.resize(1200, 800);
  window.openFile(graph);
  for (int i = 1; i < graphs.size(); ++i) window.addFile(graphs[i]);
  window.show();

  return app->exec();
}
