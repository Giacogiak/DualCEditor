#include "viewport.h"

#include <QDebug>

Viewport::Viewport(QWidget* parent) : QOpenGLWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
}

void Viewport::initializeGL() {
  if (!initializeOpenGLFunctions()) {
    qFatal("Viewport: no OpenGL 3.3 core context");
  }
  qInfo().noquote() << "GL renderer:"
                    << reinterpret_cast<const char*>(glGetString(GL_RENDERER))
                    << "| version:"
                    << reinterpret_cast<const char*>(glGetString(GL_VERSION));
  glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
}

void Viewport::resizeGL(int w, int h) {
  glViewport(0, 0, w, h);
}

void Viewport::paintGL() {
  glClear(GL_COLOR_BUFFER_BIT);
}
