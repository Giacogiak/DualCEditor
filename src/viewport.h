#pragma once

#include "field_glsl.h"
#include "orbit_camera.h"
#include "trace_renderer.h"

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QPointF>
#include <QSize>
#include <QTimer>

#include <deque>
#include <optional>
#include <vector>

// The 3D viewport: a GL 3.3 core QOpenGLWidget that sphere-traces a compiled
// field graph through DualC's generated shader (TraceRenderer), on the framed
// default view of DualC's viewers. Mouse and keys drive an OrbitCamera
// (docs/command_reference/01-viewport.md). While the view moves it traces at
// `previewScale` and upscales, then settles to full resolution kSettleMs after
// the last input, as dualc_field_view does. It paints only on a change: an
// idle viewport draws nothing new.
class Viewport : public QOpenGLWidget {
  Q_OBJECT

 public:
  static constexpr int kSettleMs = 180;  // dualc_field_view's kSettle
  static constexpr float kDefaultPreviewScale = 0.5f;

  // `previewScale` is clamped to [0.1, 1]; 1 turns the preview off. With no
  // scene the viewport shows the background until setScene succeeds.
  explicit Viewport(float previewScale = kDefaultPreviewScale, QWidget* parent = nullptr);
  explicit Viewport(dce::fieldgraph::GlslScene scene,
                    float previewScale = kDefaultPreviewScale, QWidget* parent = nullptr);
  ~Viewport() override;

  // Swap in a new scene. The trace program is linked at once when GL is up,
  // else when it comes up (initializeGL). Success emits sceneApplied and
  // repaints; `frame` reframes the camera on the new bounds, otherwise the
  // camera stays where it is (a reload). A failed link emits sceneFailed and
  // leaves the previous scene, and its image, in place.
  void setScene(dce::fieldgraph::GlslScene scene, bool frame);
  bool hasScene() const { return hasScene_; }
  const dce::fieldgraph::GlslScene& scene() const { return scene_; }

  // A parameter edit: set binding `index` of the scene on screen to `value`
  // (as many floats as the binding has) and upload that one uniform. It never
  // recompiles or relinks. The view previews and settles as for a camera move.
  // An edit lasts until the next setScene, which brings its own values.
  void setParameter(int index, std::vector<float> value);

  // Program links attempted so far, and the GL compile + link time of the
  // scene on screen, in ms.
  int linkCount() const { return renderer_.linkCount(); }
  double compileMs() const { return renderer_.compileMs(); }

  // A scripted orbit, as a left-drag of (dx, dy) pixels would make it: the
  // view previews and settles as for a mouse drag. The measurement run uses it.
  void orbitByPixels(double dx, double dy);

  // GL_RENDERER and GL_VERSION of the context, once GL is up.
  const QString& glRenderer() const { return glRenderer_; }
  const QString& glVersion() const { return glVersion_; }

  const OrbitCamera& camera() const { return camera_; }
  float previewScale() const { return previewScale_; }
  bool isGLReady() const { return glReady_; }
  int paintCount() const { return paintCount_; }

  // When on, each frame ends with glFinish so frameDrawn's `ms` is the real
  // trace time. On by default when the dce.viewport.paint log is enabled.
  void setFrameTiming(bool on) { timeFrames_ = on; }

 signals:
  // One paintGL: `traced` is the size actually traced (smaller than the widget
  // in a preview frame), `ms` the CPU time of paintGL (GPU time too when frame
  // timing is on).
  void frameDrawn(bool preview, QSize traced, double ms);
  // The GPU time of a frame, from a GL_TIME_ELAPSED query read back after the
  // frame (a few ms later, without a stall). Frames come in paint order. A
  // frame painted while every query is still in flight has no GPU time.
  void gpuFrameTimed(bool preview, QSize traced, double ms);
  void sceneApplied();
  // `error` is the shader compile or link log.
  void sceneFailed(const QString& error);

 protected:
  void initializeGL() override;
  void paintGL() override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  enum class Drag { None, Orbit, Pan };

  void releaseGL();
  // Link `scene` on the current context and take it on success.
  void linkScene(dce::fieldgraph::GlslScene scene, bool frame);
  // The view changed (camera or parameter): repaint at preview scale and
  // restart the settle wait.
  void viewChanged();
  // Read back the GPU timer queries that have finished.
  void pollGpuTimers();
  bool moving() const { return drag_ != Drag::None || settle_.isActive(); }

  dce::fieldgraph::GlslScene scene_;  // the scene on screen, once hasScene_
  bool hasScene_ = false;
  // A scene given before GL was up, and whether to frame it.
  std::optional<dce::fieldgraph::GlslScene> pending_;
  bool pendingFrame_ = false;
  OrbitCamera camera_;
  TraceRenderer renderer_;
  bool glReady_ = false;
  QString glRenderer_, glVersion_;
  float previewScale_;
  bool timeFrames_ = false;
  int paintCount_ = 0;
  bool lastFramePreview_ = false;

  Drag drag_ = Drag::None;
  Qt::MouseButton dragButton_ = Qt::NoButton;
  QPointF lastPos_;
  QTimer settle_;  // single-shot; running = inside the settle window
  QTimer gpuPoll_;  // runs while GPU timer queries are in flight
  struct TimedFrame {
    bool preview;
    QSize traced;
  };
  std::deque<TimedFrame> timedFrames_;  // one per query in flight, paint order
};
