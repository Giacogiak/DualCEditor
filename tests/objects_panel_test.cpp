// Drives the Objects dock of a live MainWindow over three graphs placed side
// by side (grey, orange, blue): two drops give objects in the panel and on
// screen; unchecking a row hides its object without a link, checking it
// brings the same image back; selecting a row selects the object; Delete
// removes it, and removing the last one empties the viewport.
//
// Needs a display and an OpenGL 3.3 core context; exits 77 (ctest: skipped)
// without them.

#include "main_window.h"
#include "objects_panel.h"
#include "viewport.h"

#include <QApplication>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QMimeData>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QUrl>
#include <QWindow>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int kExitSkip = 77;
int failures = 0;

void check(bool ok, const char* what) {
  std::fprintf(ok ? stdout : stderr, "objects_panel_test: %s %s\n", ok ? "ok  " : "FAIL",
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
    std::fprintf(stderr, "objects_panel_test: cannot write %s\n", qPrintable(path));
    std::exit(1);
  }
  f.write(text);
}

// A drag-enter then a drop of `paths` onto the window, as one drag.
bool drop(MainWindow& w, const QStringList& paths) {
  QMimeData mime;
  QList<QUrl> urls;
  for (const QString& p : paths) urls << QUrl::fromLocalFile(p);
  mime.setUrls(urls);
  const QPoint at(w.width() / 2, w.height() / 2);
  QDragEnterEvent enter(at, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(&w, &enter);
  if (!enter.isAccepted()) return false;
  QDropEvent dropped(QPointF(at), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(&w, &dropped);
  return dropped.isAccepted();
}

// Pixels of the object colours: orange (object 2) and blue (object 3).
int orange(const QImage& im) {
  int n = 0;
  for (int y = 0; y < im.height(); ++y)
    for (int x = 0; x < im.width(); ++x) {
      const QColor c = im.pixelColor(x, y);
      n += c.red() > c.blue() + 60 && c.red() > 80;
    }
  return n;
}
int blue(const QImage& im) {
  int n = 0;
  for (int y = 0; y < im.height(); ++y)
    for (int x = 0; x < im.width(); ++x) {
      const QColor c = im.pixelColor(x, y);
      n += c.blue() > c.red() + 60 && c.blue() > 80;
    }
  return n;
}

int idAt(const MainWindow& w, std::size_t i) {
  return i < w.scene().objects().size() ? w.scene().objects()[i].id : 0;
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef Q_OS_LINUX
  if (qEnvironmentVariableIsEmpty("DISPLAY") &&
      qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
    std::printf("objects_panel_test: skipped, no display\n");
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
    std::printf("objects_panel_test: skipped, platform %s has no window GL\n",
                qPrintable(platform));
    return kExitSkip;
  }

  // The three graphs of docs/command_reference/01-viewport.md § Several graphs.
  QTemporaryDir tmp;
  const QString a = tmp.filePath("a.fld"), b = tmp.filePath("b.fld"),
                c = tmp.filePath("c.fld");
  write(a, "translate(by=[-2,0,0],sphere(radius=0.8))");
  write(b, "box(min=[-0.6,-0.6,-0.6],max=[0.6,0.6,0.6])");
  write(c, "translate(by=[2,0,0],sphere(radius=0.8))");

  // Preview off: every frame is a full one, so images compare exactly.
  MainWindow w(1.0f);
  w.resize(800, 500);
  w.show();
  Viewport& vp = *w.viewport();
  ObjectsPanel& panel = *w.objects();
  if (!runUntil([&] { return w.windowHandle() && w.windowHandle()->isExposed() &&
                             vp.paintCount() > 0; },
                10000)) {
    std::fprintf(stderr, "objects_panel_test: the window never painted\n");
    return 1;
  }
  if (!vp.isGLReady()) {
    std::printf("objects_panel_test: skipped, no OpenGL 3.3 core context\n");
    return kExitSkip;
  }
  check(panel.count() == 0, "no graph, no rows");

  // Two drops, the second carrying two files: three objects.
  check(drop(w, {a}), "drop a.fld");
  runUntil([&] { return vp.hasScene(); }, 10000);
  quiet(vp);
  const QImage one = vp.grabFramebuffer();
  check(drop(w, {b, c}), "drop b.fld and c.fld together");
  runUntil([&] { return w.scene().objects().size() == 3; }, 10000);
  quiet(vp);
  check(panel.count() == 3, "three rows");
  check(panel.count() == 3 && panel.item(0)->text() == "a.fld" &&
            panel.item(1)->text() == "b.fld" && panel.item(2)->text() == "c.fld",
        "the rows are named after the files, in drop order");
  check(panel.count() == 3 && panel.item(1)->checkState() == Qt::Checked,
        "a new object is visible");
  const QImage three = vp.grabFramebuffer();
  const int orange3 = orange(three), blue3 = blue(three);
  std::printf("objects_panel_test: orange %d px, blue %d px, of %d\n", orange3, blue3,
              three.width() * three.height());
  check(orange(one) == 0 && orange3 > 500 && blue3 > 500,
        "the viewport draws the added objects in their colours");

  // Hide b from the panel: a uniform, no link; show it: the same image.
  const int links = vp.linkCount(), compiles = vp.compileCount();
  const int bId = idAt(w, 1);
  panel.row(bId)->setCheckState(Qt::Unchecked);
  quiet(vp);
  const QImage hidden = vp.grabFramebuffer();
  check(!w.scene().objects()[1].visible, "unchecking hides the object in the scene");
  check(orange(hidden) == 0 && blue(hidden) > 500, "the hidden object is gone from the image");
  panel.row(bId)->setCheckState(Qt::Checked);
  quiet(vp);
  check(vp.grabFramebuffer() == three, "checking it again restores the image exactly");
  check(vp.linkCount() == links && vp.compileCount() == compiles,
        "hiding and showing link and compile nothing");
  std::printf("objects_panel_test: links %d -> %d, compiles %d -> %d\n", links,
              vp.linkCount(), compiles, vp.compileCount());

  // Select c from the panel.
  const int cId = idAt(w, 2);
  int signalled = -1;
  QObject::connect(&w, &MainWindow::selectionChanged, [&](int id) { signalled = id; });
  panel.setCurrentItem(panel.row(cId));
  check(w.selectedObject() == cId && signalled == cId, "selecting a row selects the object");
  quiet(vp);
  check(vp.selectedObject() == cId && vp.grabFramebuffer() != three,
        "a row selected in the panel is highlighted in the viewport");

  // Ctrl+click adds a row to the selection and keeps the order (P05 T1): c,
  // then a; the last one is the selected object. Ctrl+click on c drops it.
  {
    const int aId = idAt(w, 0);
    const auto click = [&](int id, Qt::KeyboardModifiers modifiers) {
      const QPoint at = panel.visualItemRect(panel.row(id)).center();
      for (QEvent::Type type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease}) {
        QMouseEvent e(type, QPointF(at), panel.viewport()->mapToGlobal(QPointF(at)),
                      Qt::LeftButton, type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton,
                      modifiers);
        QApplication::sendEvent(panel.viewport(), &e);
      }
    };
    const auto ctrlClick = [&](int id) { click(id, Qt::ControlModifier); };
    ctrlClick(aId);
    check(panel.selectedIds() == std::vector<int>{cId, aId} && w.selectedObject() == aId,
          "Ctrl+click adds a row, in selection order");
    ctrlClick(cId);
    check(panel.selectedIds() == std::vector<int>{aId} && w.selectedObject() == aId,
          "Ctrl+click on a selected row drops it");
    ctrlClick(cId);
    click(cId, Qt::NoModifier);
    check(panel.selectedIds() == std::vector<int>{cId} && w.selectedObject() == cId,
          "a plain click on c selects it alone");
    quiet(vp);
  }

  // Click to select (P01 T3): a click on b's pixels selects b, in the panel
  // too; a click on the background clears the selection. Neither links.
  {
    const QImage before = vp.grabFramebuffer();
    double sx = 0, sy = 0;
    int n = 0;
    for (int y = 0; y < three.height(); ++y)
      for (int x = 0; x < three.width(); ++x) {
        const QColor c = three.pixelColor(x, y);
        if (c.red() > 150 && c.green() < 140 && c.blue() < 110) sx += x, sy += y, ++n;
      }
    const qreal dpr = vp.devicePixelRatioF();
    const auto click = [&](QPointF at) {
      QMouseEvent press(QEvent::MouseButtonPress, at, vp.mapToGlobal(at), Qt::LeftButton,
                        Qt::LeftButton, Qt::NoModifier);
      QCoreApplication::sendEvent(&vp, &press);
      QMouseEvent up(QEvent::MouseButtonRelease, at, vp.mapToGlobal(at), Qt::LeftButton,
                     Qt::NoButton, Qt::NoModifier);
      QCoreApplication::sendEvent(&vp, &up);
      quiet(vp);
    };
    const int bId = idAt(w, 1);
    const OrbitCamera cam = vp.camera();
    click(QPointF(sx / std::max(n, 1), sy / std::max(n, 1)) / dpr);
    check(n > 0 && w.selectedObject() == bId && signalled == bId &&
              panel.selectedId() == bId,
          "clicking an object selects it, in the panel too");
    check(vp.selectedObject() == bId && vp.grabFramebuffer() != before,
          "the clicked object is highlighted");
    check(vp.camera().yaw == cam.yaw && vp.camera().pitch == cam.pitch,
          "a click does not turn the view");
    click(QPointF(4, 4));
    check(w.selectedObject() == 0 && signalled == 0 && panel.selectedId() == 0,
          "clicking empty space clears the selection");
    check(vp.grabFramebuffer() == three, "with no selection the image is the unselected one");
    check(vp.linkCount() == links && vp.compileCount() == compiles,
          "selecting links and compiles nothing");
    panel.setCurrentItem(panel.row(cId));
    check(w.selectedObject() == cId, "the panel selects again");
  }

  // Delete it with the Delete key on the panel.
  panel.setFocus();
  QKeyEvent del(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
  QCoreApplication::sendEvent(&panel, &del);
  runUntil([&] { return w.scene().objects().size() == 2; }, 10000);
  quiet(vp);
  check(w.scene().objects().size() == 2 && panel.count() == 2 && !panel.row(cId),
        "Delete removes the selected object and its row");
  check(w.selectedObject() == 0 && signalled == 0, "the selection is cleared");
  const QImage two = vp.grabFramebuffer();
  check(blue(two) == 0 && orange(two) > 500, "the deleted object is gone from the image");

  // Delete the rest: the viewport is empty.
  for (int id : {idAt(w, 0), idAt(w, 1)}) {
    panel.setCurrentItem(panel.row(id));
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
    QCoreApplication::sendEvent(&panel, &key);
    runUntil([&] { return !w.scene().find(id); }, 10000);
  }
  quiet(vp);
  check(w.scene().isEmpty() && panel.count() == 0 && !vp.hasScene(),
        "deleting the last object empties the scene and the viewport");
  check(orange(vp.grabFramebuffer()) == 0 && blue(vp.grabFramebuffer()) == 0,
        "the empty viewport shows the background");

  if (failures) {
    std::fprintf(stderr, "objects_panel_test: %d check(s) failed\n", failures);
    return 1;
  }
  std::printf("objects_panel_test: all checks passed\n");
  return 0;
}
