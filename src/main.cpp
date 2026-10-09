#include <QApplication>
#include <QMainWindow>
#include "viewport.h"

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  QMainWindow window;
  window.setWindowTitle("DualCEditor");
  window.resize(1200, 800);

  Viewport *viewport = new Viewport(&window);
  window.setCentralWidget(viewport);

  window.show();
  return app.exec();
}
