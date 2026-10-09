#include "viewport.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <string>
#include <utility>

Viewport::Viewport(dce::fieldgraph::GlslScene scene, QWidget* parent)
    : QOpenGLWidget(parent), scene_(std::move(scene)) {
  setFocusPolicy(Qt::StrongFocus);
  frameBounds(camera_, scene_.bounds);
}

Viewport::~Viewport() {
  releaseGL();
}

void Viewport::initializeGL() {
  // The context is destroyed with the widget or on reparenting; the GL objects
  // go with it.
  connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &Viewport::releaseGL,
          Qt::UniqueConnection);
  if (!renderer_.initialize()) {
    const QSurfaceFormat f = context()->format();
    qCritical().noquote() << "Viewport: need an OpenGL 3.3 core context, got"
                          << QString("%1.%2").arg(f.majorVersion()).arg(f.minorVersion());
    return;
  }
  glReady_ = true;
  qInfo().noquote() << "GL renderer:"
                    << reinterpret_cast<const char*>(
                           context()->functions()->glGetString(GL_RENDERER));
  std::string error;
  if (!renderer_.setScene(scene_, &error))
    qCritical().noquote() << "Viewport:" << QString::fromStdString(error);
}

void Viewport::paintGL() {
  if (!glReady_) return;
  const qreal dpr = devicePixelRatioF();
  renderer_.render(qRound(width() * dpr), qRound(height() * dpr), camera_);
}

void Viewport::releaseGL() {
  if (!glReady_ || !context()) return;
  makeCurrent();
  renderer_.release();
  doneCurrent();
  glReady_ = false;
}
