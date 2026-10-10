#include "viewport.h"

#include <QDebug>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QWheelEvent>

#include <algorithm>
#include <string>
#include <utility>

// One line per paintGL. Off by default; enable with
// QT_LOGGING_RULES="dce.viewport.paint.debug=true".
Q_LOGGING_CATEGORY(lcPaint, "dce.viewport.paint", QtInfoMsg)

Viewport::Viewport(dce::fieldgraph::GlslScene scene, float previewScale, QWidget* parent)
    : QOpenGLWidget(parent),
      scene_(std::move(scene)),
      previewScale_(std::clamp(previewScale, 0.1f, 1.0f)) {
  setFocusPolicy(Qt::StrongFocus);
  frameBounds(camera_, scene_.bounds);
  timeFrames_ = lcPaint().isDebugEnabled();
  settle_.setSingleShot(true);
  settle_.setTimerType(Qt::PreciseTimer);
  settle_.setInterval(kSettleMs);
  // The settle window closed: one full-resolution frame, unless a drag is
  // still held (dualc_field_view keeps previewing until the button is up) or
  // the last frame already was full resolution.
  connect(&settle_, &QTimer::timeout, this, [this] {
    if (drag_ == Drag::None && lastFramePreview_) update();
  });
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
  // Open inside the settle window, as dualc_field_view does: the first frame
  // is a quick preview instead of a heavy full-resolution trace.
  settle_.start();
}

void Viewport::paintGL() {
  if (!glReady_) return;
  QElapsedTimer clock;
  clock.start();
  const qreal dpr = devicePixelRatioF();
  const int w = qRound(width() * dpr), h = qRound(height() * dpr);
  const bool preview = moving() && previewScale_ < 0.999f;
  QSize traced(w, h);
  if (preview)
    traced = renderer_.renderPreview(defaultFramebufferObject(), w, h, previewScale_,
                                     camera_);
  else
    renderer_.render(w, h, camera_);
  if (timeFrames_) renderer_.finish();
  const double ms = clock.nsecsElapsed() * 1e-6;
  lastFramePreview_ = preview;
  ++paintCount_;
  qCDebug(lcPaint).noquote() << QString("paintGL #%1 %2 %3x%4 %5 ms")
                                    .arg(paintCount_)
                                    .arg(preview ? "preview" : "full")
                                    .arg(traced.width())
                                    .arg(traced.height())
                                    .arg(ms, 0, 'f', 1);
  emit frameDrawn(preview, traced, ms);
}

void Viewport::cameraChanged() {
  settle_.start();
  update();
}

void Viewport::mousePressEvent(QMouseEvent* event) {
  const bool shift = event->modifiers() & Qt::ShiftModifier;
  if (event->button() == Qt::LeftButton)
    drag_ = shift ? Drag::Pan : Drag::Orbit;
  else if (event->button() == Qt::MiddleButton)
    drag_ = Drag::Pan;
  else
    return QOpenGLWidget::mousePressEvent(event);
  // Nothing moved yet, so nothing repaints; the next frame is a preview
  // because a drag is held.
  dragButton_ = event->button();
  lastPos_ = event->position();
}

void Viewport::mouseMoveEvent(QMouseEvent* event) {
  if (drag_ == Drag::None) return QOpenGLWidget::mouseMoveEvent(event);
  const QPointF d = event->position() - lastPos_;
  lastPos_ = event->position();
  if (drag_ == Drag::Orbit)
    orbitBy(camera_, d.x(), d.y());
  else
    panBy(camera_, d.x(), d.y(), height());
  cameraChanged();
}

void Viewport::mouseReleaseEvent(QMouseEvent* event) {
  // The drag ends when its button comes up; the settle wait starts there.
  if (drag_ == Drag::None || event->button() != dragButton_)
    return QOpenGLWidget::mouseReleaseEvent(event);
  drag_ = Drag::None;
  settle_.start();
}

void Viewport::wheelEvent(QWheelEvent* event) {
  // 120 eighths of a degree is one notch, GLFW's scroll unit of 1.
  const double steps = event->angleDelta().y() / 120.0;
  if (steps == 0.0) return QOpenGLWidget::wheelEvent(event);
  dollyBy(camera_, steps);
  cameraChanged();
}

void Viewport::keyPressEvent(QKeyEvent* event) {
  // Keys are matched by the character they type, never by key position, so
  // they follow the user's layout (the owner types on Belgian AZERTY).
  if (event->text().compare("f", Qt::CaseInsensitive) == 0) {
    frameBounds(camera_, scene_.bounds);
    cameraChanged();
    return;
  }
  QOpenGLWidget::keyPressEvent(event);
}

void Viewport::releaseGL() {
  if (!glReady_ || !context()) return;
  makeCurrent();
  renderer_.release();
  doneCurrent();
  glReady_ = false;
}
