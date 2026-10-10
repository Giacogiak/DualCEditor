#include "gpu_preference.h"  // Optimus/PowerXpress opt-in (Windows); one TU only
#include "graph_file.h"
#include "main_window.h"
#include "snapshot.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QSurfaceFormat>

#include <cstdio>
#include <cstring>
#include <memory>

namespace {

// The graph the window opens on when no file is named: DualC's
// examples/samples/gyroid_box.json, embedded at build time.
constexpr const char* kDefaultGraph = ":/samples/gyroid_box.json";

bool wantsSnapshot(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i)
    if (std::strncmp(argv[i], "--snapshot", 10) == 0) return true;
  return false;
}

}  // namespace

int main(int argc, char* argv[]) {
  // The sphere-tracer DualC generates is GLSL 330 core; ask for that context
  // before any widget or surface exists.
  QSurfaceFormat format;
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(format);

  // A snapshot opens no window, so it needs no widgets, and on a Linux box
  // with no display it falls back to the offscreen platform rather than
  // aborting: there it reports "no GL context" and the ctest skips.
  const bool snapshot = wantsSnapshot(argc, argv);
#ifdef Q_OS_LINUX
  if (snapshot && qEnvironmentVariableIsEmpty("DISPLAY") &&
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
  parser.addPositionalArgument(
      "graph", "Field graph to open (.json or .fld shorthand). Default: gyroid_box.json.",
      "[graph]");
  parser.process(*app);
  if (parser.positionalArguments().size() > 1) {
    std::fprintf(stderr, "DualCEditor: one graph at most, got %d\n",
                 int(parser.positionalArguments().size()));
    return 1;
  }
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

  if (parser.isSet(snapshotOption)) {
    GraphLoad load = loadGraphFile(graph);
    if (!load.scene) {
      std::fprintf(stderr, "DualCEditor: %s\n", qPrintable(load.error));
      return 2;
    }
    return renderSnapshot(*load.scene, parser.value(snapshotOption), 800, 600);
  }

  // A graph that does not load still opens the window: the error is in the
  // status bar and the log, and the file is watched, so fixing it renders it.
  MainWindow window(previewScale);
  window.resize(1200, 800);
  window.openFile(graph);
  window.show();

  return app->exec();
}
