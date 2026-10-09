#include "gpu_preference.h"  // Optimus/PowerXpress opt-in (Windows); one TU only
#include "viewport.h"

#include <QApplication>
#include <QMainWindow>
#include <QSurfaceFormat>

int main(int argc, char* argv[]) {
  // The sphere-tracer DualC generates is GLSL 330 core; ask for that context
  // before any widget exists.
  QSurfaceFormat format;
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(format);

  QApplication app(argc, argv);

  QMainWindow window;
  window.setWindowTitle("DualCEditor");
  window.resize(1200, 800);
  window.setCentralWidget(new Viewport(&window));
  window.show();

  return app.exec();
}
