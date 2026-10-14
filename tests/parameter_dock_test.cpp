// Drives the Parameters dock of a live MainWindow on gyroid_box.json: the dock
// lists every binding at its value; editing the gyroid wavelength and the
// onion thickness changes the image without a program link (the link count
// stays put); restoring them restores the image exactly; the status bar shows
// the compile time and the GPU time of the last settled frame. A save that
// changes the file relinks once and the dock follows the new values. A vec3
// and a mat4 binding get three and sixteen editors.
//
// Needs a display and an OpenGL 3.3 core context; exits 77 (ctest: skipped)
// without them.
//
// usage: parameter_dock_test <gyroid_box.json>

#include "main_window.h"
#include "parameter_panel.h"
#include "viewport.h"

#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QRegularExpression>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QWindow>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

constexpr int kExitSkip = 77;
int failures = 0;

void check(bool ok, const char* what) {
  std::fprintf(ok ? stdout : stderr, "parameter_dock_test: %s %s\n",
               ok ? "ok  " : "FAIL", what);
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
    std::fprintf(stderr, "parameter_dock_test: cannot write %s\n", qPrintable(path));
    std::exit(1);
  }
  f.write(text);
}

// The index of the binding driven by `key`, or -1.
int bindingIndex(const Viewport& vp, const char* key) {
  const auto& bindings = vp.scene().bindings;
  for (std::size_t i = 0; i < bindings.size(); ++i)
    if (bindings[i].paramKey == key) return int(i);
  return -1;
}

// Every binding has one editor per float, set to its value.
bool dockMatchesScene(MainWindow& w) {
  const auto& bindings = w.viewport()->scene().bindings;
  for (std::size_t i = 0; i < bindings.size(); ++i) {
    const int n = int(bindings[i].value.size());
    for (int k = 0; k < n; ++k) {
      const QDoubleSpinBox* box = w.parameters()->editor(int(i), k);
      if (!box || float(box->value()) != bindings[i].value[std::size_t(k)]) return false;
    }
    if (w.parameters()->editor(int(i), n)) return false;
  }
  return w.parameters()->editor(int(bindings.size())) == nullptr;
}

// Set an editor as the user would with its arrows or Enter, and let the
// preview settle.
void edit(MainWindow& w, int index, double value) {
  w.parameters()->editor(index)->setValue(value);
  quiet(*w.viewport());
}

constexpr const char* kTransformFld =
    "translate(sphere(center=[0,0,0],radius=1),by=[0.5,0,0])\n";

}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: parameter_dock_test <gyroid_box.json>\n");
    return 2;
  }
#ifdef Q_OS_LINUX
  if (qEnvironmentVariableIsEmpty("DISPLAY") &&
      qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
    std::printf("parameter_dock_test: skipped, no display\n");
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
    std::printf("parameter_dock_test: skipped, platform %s has no window GL\n",
                qPrintable(platform));
    return kExitSkip;
  }
  QFile source(QString::fromLocal8Bit(argv[1]));
  if (!source.open(QIODevice::ReadOnly)) {
    std::fprintf(stderr, "parameter_dock_test: cannot read %s\n", argv[1]);
    return 1;
  }
  const QByteArray gyroid = source.readAll();

  QTemporaryDir tmp;
  const QString json = tmp.filePath("gyroid_box.json");
  write(json, gyroid);

  // Preview off: every frame is a full one, so images compare exactly.
  MainWindow w(1.0f);
  Viewport& vp = *w.viewport();
  w.resize(900, 600);
  w.show();
  if (!w.windowHandle() ||
      !runUntil([&] { return w.windowHandle()->isExposed(); }, 10000)) {
    std::printf("parameter_dock_test: skipped, the window never showed\n");
    return kExitSkip;
  }
  runUntil([&] { return vp.isGLReady() || vp.paintCount() > 0; }, 10000);
  if (!vp.isGLReady()) {
    std::printf("parameter_dock_test: skipped, no OpenGL 3.3 core context\n");
    return kExitSkip;
  }
  check(w.parameters()->editor(0) == nullptr, "no graph, no parameters");

  // Count the settled (full-resolution) frames the GPU timer reports.
  int fullTimed = 0;
  double lastFullMs = -1.0;
  QObject::connect(&vp, &Viewport::gpuFrameTimed, [&](bool preview, QSize, double ms) {
    if (preview) return;
    ++fullTimed;
    lastFullMs = ms;
  });

  w.openFile(json);
  runUntil([&] { return vp.hasScene(); }, 20000);
  quiet(vp);
  runUntil([&] { return fullTimed > 0; }, 5000);
  check(vp.hasScene(), "gyroid_box.json is on screen");
  check(dockMatchesScene(w), "the dock has one editor per binding float, at its value");

  const int wavelength = bindingIndex(vp, "wavelength");
  const int thickness = bindingIndex(vp, "thickness");
  check(wavelength >= 0 && thickness >= 0, "the scene binds wavelength and thickness");
  if (wavelength < 0 || thickness < 0) return 1;

  // The status bar: compile ms and the GPU ms of the settled frame.
  const QRegularExpression timing(
      R"(^compile (\d+\.\d) ms   frame (\d+\.\d) ms$)");
  const QRegularExpressionMatch m = timing.match(w.timingText());
  std::printf("parameter_dock_test: status bar \"%s\"\n", qPrintable(w.timingText()));
  check(m.hasMatch(), "the status bar shows the compile and frame ms");
  check(vp.compileMs() > 0.0 &&
            m.captured(1) == QString::number(vp.compileMs(), 'f', 1),
        "the compile ms is the link of the scene on screen");
  check(fullTimed > 0 && m.captured(2) == QString::number(lastFullMs, 'f', 1),
        "the frame ms is the GPU time of the last settled frame");

  const int links = vp.linkCount();
  const QImage original = vp.grabFramebuffer();

  // Uniform-only edits: the image changes, no program is linked.
  int painted = vp.paintCount();
  edit(w, wavelength, 0.6);
  check(vp.scene().bindings[std::size_t(wavelength)].value[0] == 0.6f,
        "a wavelength edit reaches the scene's binding");
  check(vp.paintCount() > painted, "a wavelength edit repaints");
  const QImage waved = vp.grabFramebuffer();
  check(waved != original, "a wavelength edit changes the image");
  check(vp.linkCount() == links, "a wavelength edit links no program");

  painted = vp.paintCount();
  const int timedBefore = fullTimed;
  edit(w, thickness, 0.12);
  check(vp.paintCount() > painted, "a thickness edit repaints");
  check(vp.grabFramebuffer() != waved, "a thickness edit changes the image");
  check(vp.linkCount() == links, "a thickness edit links no program");
  runUntil([&] { return fullTimed > timedBefore; }, 5000);
  check(w.timingText().endsWith(QString("frame %1 ms").arg(lastFullMs, 0, 'f', 1)),
        "the frame ms follows the new settled frame");

  // Back to the file's values: the image is the original one, pixel for pixel.
  edit(w, wavelength, 1.0);
  edit(w, thickness, 0.3);
  check(vp.grabFramebuffer() == original, "restoring the values restores the image");
  check(vp.linkCount() == links, "after four edits, still no link");

  // An edit to the same value paints nothing.
  painted = vp.paintCount();
  w.parameters()->editor(thickness)->setValue(0.3);
  quiet(vp);
  check(vp.paintCount() == painted, "an edit to the same value paints nothing");

  // A save with a new wavelength: one link, and the dock follows the file.
  QByteArray edited = gyroid;
  edited.replace("\"wavelength\": 1.0", "\"wavelength\": 1.5");
  check(edited != gyroid, "the test can edit the wavelength in the file");
  write(json, edited);
  runUntil([&] { return vp.linkCount() > links; }, 10000);
  quiet(vp);
  check(vp.linkCount() == links + 1, "a changed save links once");
  check(w.parameters()->editor(wavelength) &&
            w.parameters()->editor(wavelength)->value() == 1.5,
        "the dock shows the reloaded wavelength");
  check(dockMatchesScene(w), "the dock matches the reloaded scene");

  // A vec3 and a mat4.
  const QString fld = tmp.filePath("moved.fld");
  write(fld, kTransformFld);
  w.openFile(fld);
  runUntil([&] { return vp.linkCount() > links + 1; }, 10000);
  quiet(vp);
  const int center = bindingIndex(vp, "center");
  const int mat = bindingIndex(vp, "mat");
  check(center >= 0 && mat >= 0, "the .fld binds a vec3 center and a mat4");
  check(dockMatchesScene(w), "a vec3 gets three editors and a mat4 sixteen");
  if (center >= 0) {
    const int before = vp.linkCount();
    const QImage centred = vp.grabFramebuffer();
    w.parameters()->editor(center, 1)->setValue(0.4);
    quiet(vp);
    check(vp.scene().bindings[std::size_t(center)].value[1] == 0.4f,
          "a vec3 component edit reaches its binding");
    check(vp.grabFramebuffer() != centred, "a vec3 edit changes the image");
    check(vp.linkCount() == before, "a vec3 edit links no program");
  }

  // The Section toolbar: X on cuts the image at the box's middle, + moves the
  // cut, off restores the image; one toggle at a time; no link.
  {
    quiet(vp);
    const int links = vp.linkCount();
    const QImage whole = vp.grabFramebuffer();
    w.sectionAxisAction(0)->setChecked(true);
    quiet(vp);
    check(vp.section().on[0] && !vp.section().on[1] && !vp.section().on[2],
          "the X toggle turns the x section on");
    const QImage cut = vp.grabFramebuffer();
    check(cut != whole, "the x section cuts the image");
    w.sectionMoveAction(+1)->trigger();
    quiet(vp);
    check(std::abs(vp.section().pos01[0] - 0.55f) < 1e-6f && vp.grabFramebuffer() != cut,
          "+ moves the section a twentieth of the box and repaints");
    w.sectionAxisAction(2)->setChecked(true);
    check(vp.section().on[2] && !vp.section().on[0], "the Z toggle replaces the x cut");
    w.sectionAxisAction(2)->setChecked(false);
    quiet(vp);
    check(!vp.section().any() && vp.grabFramebuffer() == whole,
          "the section off restores the image exactly");
    check(vp.linkCount() == links, "the section links no program");
  }

  if (failures) {
    std::fprintf(stderr, "parameter_dock_test: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("parameter_dock_test: all checks passed\n");
  return 0;
}
