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
    : Viewport(previewScale, parent) {
  setScene(std::move(scene), true);
}

Viewport::Viewport(float previewScale, QWidget* parent)
    : QOpenGLWidget(parent), previewScale_(std::clamp(previewScale, 0.1f, 1.0f)) {
  setFocusPolicy(Qt::StrongFocus);
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
  gpuPoll_.setInterval(2);
  connect(&gpuPoll_, &QTimer::timeout, this, &Viewport::pollGpuTimers);
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
    if (pending_) {
      pending_.reset();
      emit sceneFailed("no OpenGL 3.3 core context");
    }
    return;
  }
  glReady_ = true;
  QOpenGLFunctions* gl = context()->functions();
  glRenderer_ = QString::fromLatin1(reinterpret_cast<const char*>(gl->glGetString(GL_RENDERER)));
  glVersion_ = QString::fromLatin1(reinterpret_cast<const char*>(gl->glGetString(GL_VERSION)));
  qInfo().noquote() << "GL renderer:" << glRenderer_;
  if (pending_) {
    std::optional<dce::fieldgraph::GlslScene> scene = std::move(pending_);
    pending_.reset();
    linkScene(std::move(*scene), pendingFrame_);
  } else if (hasScene_) {
    // A new context (reparenting): relink the scene on screen.
    std::string error;
    if (!renderer_.setScene(scene_, &error))
      qCritical().noquote() << "Viewport:" << QString::fromStdString(error);
  }
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
  const bool timed = renderer_.beginGpuTimer();
  QSize traced(w, h);
  if (preview)
    traced = renderer_.renderPreview(defaultFramebufferObject(), w, h, previewScale_,
                                     camera_);
  else
    renderer_.render(w, h, camera_);
  if (timed) {
    renderer_.endGpuTimer();
    timedFrames_.push_back({preview, traced});
    gpuPoll_.start();
  }
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

void Viewport::setScene(dce::fieldgraph::GlslScene scene, bool frame) {
  if (!context()) {
    // GL is not up yet: initializeGL links it. A later scene replaces an
    // earlier pending one; a frame request is kept.
    pendingFrame_ = pending_ ? pendingFrame_ || frame : frame;
    pending_ = std::move(scene);
    return;
  }
  if (!glReady_) {
    emit sceneFailed("no OpenGL 3.3 core context");
    return;
  }
  makeCurrent();
  linkScene(std::move(scene), frame);
  doneCurrent();
}

void Viewport::linkScene(dce::fieldgraph::GlslScene scene, bool frame) {
  std::string error;
  if (!renderer_.setScene(scene, &error)) {
    // The previous program, grids and image stay: nothing repaints.
    emit sceneFailed(QString::fromStdString(error));
    return;
  }
  const bool first = !hasScene_;
  scene_ = std::move(scene);
  hasScene_ = true;
  if (frame || first) {
    frameBounds(camera_, scene_.bounds);
    viewChanged();
  } else {
    update();
  }
  emit sceneApplied();
}

void Viewport::setParameter(int index, std::vector<float> value) {
  if (!hasScene_ || index < 0 || index >= int(scene_.bindings.size())) return;
  dce::fieldgraph::UniformBinding& b = scene_.bindings[std::size_t(index)];
  if (value.size() != b.value.size() || value == b.value) return;
  b.value = std::move(value);
  if (!glReady_) return;
  makeCurrent();
  renderer_.setParameter(b);
  doneCurrent();
  // dualc_field_view counts a parameter nudge as an interaction too.
  viewChanged();
}

void Viewport::pollGpuTimers() {
  if (!glReady_) return gpuPoll_.stop();
  std::vector<std::pair<TimedFrame, double>> done;
  makeCurrent();
  double ms = 0.0;
  while (renderer_.takeGpuTime(&ms)) {
    done.emplace_back(timedFrames_.front(), ms);
    timedFrames_.pop_front();
  }
  if (!renderer_.gpuTimersInFlight()) gpuPoll_.stop();
  doneCurrent();
  for (const auto& [frame, gpuMs] : done) {
    qCDebug(lcPaint).noquote() << QString("gpu %1 %2x%3 %4 ms")
                                      .arg(frame.preview ? "preview" : "full")
                                      .arg(frame.traced.width())
                                      .arg(frame.traced.height())
                                      .arg(gpuMs, 0, 'f', 1);
    emit gpuFrameTimed(frame.preview, frame.traced, gpuMs);
  }
}

void Viewport::orbitByPixels(double dx, double dy) {
  orbitBy(camera_, dx, dy);
  viewChanged();
}

void Viewport::viewChanged() {
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
  viewChanged();
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
  viewChanged();
}

void Viewport::keyPressEvent(QKeyEvent* event) {
  // Keys are matched by the character they type, never by key position, so
  // they follow the user's layout (the owner types on Belgian AZERTY).
  if (event->text().compare("f", Qt::CaseInsensitive) == 0 && hasScene_) {
    frameBounds(camera_, scene_.bounds);
    viewChanged();
    return;
  }
  QOpenGLWidget::keyPressEvent(event);
}

void Viewport::releaseGL() {
  if (!glReady_ || !context()) return;
  makeCurrent();
  renderer_.release();
  doneCurrent();
  gpuPoll_.stop();
  timedFrames_.clear();
  glReady_ = false;
}
