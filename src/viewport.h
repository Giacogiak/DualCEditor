#pragma once

#include "field_glsl.h"
#include "orbit_camera.h"
#include "trace_renderer.h"

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QPointF>
#include <QSize>
#include <QTimer>

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

  // `previewScale` is clamped to [0.1, 1]; 1 turns the preview off.
  explicit Viewport(dce::fieldgraph::GlslScene scene,
                    float previewScale = kDefaultPreviewScale, QWidget* parent = nullptr);
  ~Viewport() override;

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
  // The camera changed: repaint at preview scale and restart the settle wait.
  void cameraChanged();
  bool moving() const { return drag_ != Drag::None || settle_.isActive(); }

  dce::fieldgraph::GlslScene scene_;
  OrbitCamera camera_;
  TraceRenderer renderer_;
  bool glReady_ = false;
  float previewScale_;
  bool timeFrames_ = false;
  int paintCount_ = 0;
  bool lastFramePreview_ = false;

  Drag drag_ = Drag::None;
  Qt::MouseButton dragButton_ = Qt::NoButton;
  QPointF lastPos_;
  QTimer settle_;  // single-shot; running = inside the settle window
};
