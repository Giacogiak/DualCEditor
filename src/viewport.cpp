#include "viewport.h"

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QThread>
#include <QWheelEvent>

#include <algorithm>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <string>
#include <utility>

// One line per paintGL. Off by default; enable with
// QT_LOGGING_RULES="dce.viewport.paint.debug=true".
Q_LOGGING_CATEGORY(lcPaint, "dce.viewport.paint", QtInfoMsg)

namespace fg = dce::fieldgraph;

struct Viewport::Build {
  SceneMaker make;
  SceneFinish finish;
  bool frame = false;
  int ticket = 0;
};

// One thread with its own context, in the widget context's share group: a
// program linked there is drawn by the widget. It takes the newest build
// waiting; one superseded before it starts is never run.
struct Viewport::Compiler {
  QOffscreenSurface surface;
  QOpenGLContext* context = nullptr;
  QThread* thread = nullptr;
  std::mutex mutex;
  std::condition_variable wake;
  std::shared_ptr<Build> next;
  bool quit = false;
};

Viewport::Viewport(fg::GlslScene scene, float previewScale, QWidget* parent)
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
    if (pending_ || build_) {
      pending_.reset();
      build_.reset();
      building_ = false;
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
  // A build asked for before GL was up, or cut off by a context change.
  if (build_) startBuild();
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
    build_.reset();
    building_ = false;
    return;
  }
  ++ticket_;
  build_.reset();
  building_ = false;
  if (!glReady_) {
    emit sceneFailed("no OpenGL 3.3 core context");
    return;
  }
  makeCurrent();
  linkScene(std::move(scene), frame);
  doneCurrent();
}

void Viewport::buildScene(SceneMaker make, bool frame, SceneFinish finish) {
  pending_.reset();
  // A frame asked by a build this one replaces is kept.
  const bool keepFrame = build_ && build_->frame;
  build_ = std::make_shared<Build>();
  build_->make = std::move(make);
  build_->finish = std::move(finish);
  build_->frame = frame || keepFrame;
  building_ = true;
  if (!context()) return;  // initializeGL starts it
  if (!glReady_) {
    build_.reset();
    building_ = false;
    emit sceneFailed("no OpenGL 3.3 core context");
    return;
  }
  startBuild();
}

void Viewport::startBuild() {
  if (!compiler_) {
    auto c = std::make_unique<Compiler>();
    c->surface.setFormat(context()->format());
    c->surface.create();
    c->context = new QOpenGLContext;
    c->context->setFormat(context()->format());
    c->context->setShareContext(context());
    if (!c->surface.isValid() || !c->context->create()) {
      delete c->context;
      build_.reset();
      building_ = false;
      emit sceneFailed("cannot create the shader compile context");
      return;
    }
    Compiler* raw = c.get();
    c->thread = QThread::create([this, raw] {
      QOpenGLContext* ctx = raw->context;
      QOpenGLFunctions_3_3_Core gl;
      const bool ready = ctx->makeCurrent(&raw->surface) && gl.initializeOpenGLFunctions();
      for (;;) {
        std::shared_ptr<Build> b;
        {
          std::unique_lock<std::mutex> lock(raw->mutex);
          raw->wake.wait(lock, [raw] { return raw->quit || raw->next; });
          if (raw->quit) break;
          b = std::move(raw->next);
        }
        // DualC's codegen, then the GL compile and link, timed as setScene.
        auto scene = std::make_shared<fg::GlslScene>();
        QString error;
        GLuint program = 0;
        int compiles = 0, links = 0;
        double ms = 0.0;
        try {
          *scene = b->make();
        } catch (const std::exception& e) {
          error = QString::fromUtf8(e.what());
        }
        if (error.isEmpty() && !ready) error = "the shader compile context is not current";
        if (error.isEmpty()) {
          std::string log;
          program = TraceRenderer::linkProgram(gl, *scene, &log, &compiles, &links, &ms);
          // The widget's context sees a finished program only.
          if (program) gl.glFinish();
          else error = QString::fromStdString(log);
        }
        const int ticket = b->ticket;
        QMetaObject::invokeMethod(
            this,
            [this, ticket, program, scene, error, compiles, links, ms] {
              buildDone(ticket, program, std::move(*scene), error, compiles, links, ms);
            },
            Qt::QueuedConnection);
      }
      ctx->doneCurrent();
      ctx->moveToThread(QCoreApplication::instance()->thread());
    });
    c->context->moveToThread(c->thread);
    c->thread->start();
    compiler_ = std::move(c);
  }
  build_->ticket = ++ticket_;
  {
    std::lock_guard<std::mutex> lock(compiler_->mutex);
    compiler_->next = build_;
  }
  compiler_->wake.notify_one();
  emit buildStarted();
}

void Viewport::buildDone(int ticket, GLuint program, fg::GlslScene scene, QString error,
                         int compiles, int links, double ms) {
  renderer_.countElsewhere(compiles, links);
  // The context went, and the program with it.
  if (!glReady_ || ticket <= deadTicket_) return;
  if (!build_ || ticket != build_->ticket) {
    // Superseded: never shown.
    if (program) {
      makeCurrent();
      renderer_.dropProgram(program);
      doneCurrent();
    }
    return;
  }
  const std::shared_ptr<Build> b = std::move(build_);
  building_ = false;
  if (!program) {
    // The previous program, grids and image stay: nothing repaints.
    emit sceneFailed(error);
    return;
  }
  if (b->finish) b->finish(scene);
  // Program, grids, bindings and trace constants change together, between
  // two frames: no frame draws one scene's program with the other's values.
  makeCurrent();
  renderer_.adoptProgram(program, scene, ms);
  doneCurrent();
  takeScene(std::move(scene), b->frame);
}

void Viewport::stopCompiler() {
  if (!compiler_) return;
  {
    std::lock_guard<std::mutex> lock(compiler_->mutex);
    compiler_->quit = true;
    compiler_->next.reset();
  }
  compiler_->wake.notify_one();
  // A compile under way finishes first: a GL link cannot be cut short.
  compiler_->thread->wait();
  delete compiler_->thread;
  delete compiler_->context;
  compiler_.reset();
}

void Viewport::clearScene() {
  pending_.reset();
  ++ticket_;
  build_.reset();
  building_ = false;
  if (!hasScene_) return;
  hasScene_ = false;
  scene_ = {};
  if (glReady_) {
    makeCurrent();
    renderer_.clearScene();
    doneCurrent();
  }
  update();
}

void Viewport::linkScene(dce::fieldgraph::GlslScene scene, bool frame) {
  std::string error;
  if (!renderer_.setScene(scene, &error)) {
    // The previous program, grids and image stay: nothing repaints.
    emit sceneFailed(QString::fromStdString(error));
    return;
  }
  takeScene(std::move(scene), frame);
}

void Viewport::takeScene(fg::GlslScene scene, bool frame) {
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

void Viewport::setSelectedObject(int id) {
  if (id == selected_) return;
  selected_ = id;
  renderer_.setSelectedObject(id);
  update();
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
  lastPos_ = pressPos_ = event->position();
  dragged_ = false;
}

void Viewport::mouseMoveEvent(QMouseEvent* event) {
  if (drag_ == Drag::None) return QOpenGLWidget::mouseMoveEvent(event);
  // A click may shake by a pixel or two: the drag starts past the platform's
  // drag distance, so a click does not turn the view.
  if (!dragged_) {
    if ((event->position() - pressPos_).manhattanLength() < QApplication::startDragDistance())
      return;
    dragged_ = true;
  }
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
  // A plain left click: Shift+left is a pan, kept for later selection keys.
  const bool click = !dragged_ && drag_ == Drag::Orbit;
  drag_ = Drag::None;
  if (click) {
    emit clicked(event->position());
    return;
  }
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
  // A build in flight starts again on the next context (initializeGL); what
  // the stopped thread sent back belongs to this context's share group.
  stopCompiler();
  deadTicket_ = ticket_;
  makeCurrent();
  renderer_.release();
  doneCurrent();
  gpuPoll_.stop();
  timedFrames_.clear();
  glReady_ = false;
}
