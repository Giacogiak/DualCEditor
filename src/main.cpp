#include "gpu_preference.h"  // Optimus/PowerXpress opt-in (Windows); one TU only
#include "graph_scene.h"
#include "snapshot.h"
#include "viewport.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QGuiApplication>
#include <QMainWindow>
#include <QSurfaceFormat>

#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

namespace fg = dce::fieldgraph;

namespace {

// The graph the viewport opens on until files can be opened (plan/10_P00 T3):
// DualC's examples/samples/gyroid_box.json, embedded at build time.
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
  parser.process(*app);

  bool scaleOk = false;
  const float previewScale = parser.value(previewOption).toFloat(&scaleOk);
  if (!scaleOk) {
    std::fprintf(stderr, "DualCEditor: --preview-scale needs a number, got '%s'\n",
                 qPrintable(parser.value(previewOption)));
    return 1;
  }

  QFile file(kDefaultGraph);
  if (!file.open(QIODevice::ReadOnly)) {
    std::fprintf(stderr, "DualCEditor: cannot read %s\n", kDefaultGraph);
    return 1;
  }
  fg::GlslScene scene;
  try {
    fg::FileMeshResolver meshes;
    scene = compileGraphText(file.readAll().toStdString(), true, meshes);
  } catch (const fg::GraphError& e) {
    std::fprintf(stderr, "DualCEditor: field-graph error%s%s: %s\n",
                 e.pointer().empty() ? "" : " at ", e.pointer().c_str(), e.what());
    return 2;
  }

  if (parser.isSet(snapshotOption))
    return renderSnapshot(scene, parser.value(snapshotOption), 800, 600);

  QMainWindow window;
  window.setWindowTitle("DualCEditor");
  window.resize(1200, 800);
  window.setCentralWidget(new Viewport(std::move(scene), previewScale, &window));
  window.show();

  return app->exec();
}
