#pragma once

#include "field_glsl.h"
#include "gizmo.h"
#include "orbit_camera.h"
#include "trace_renderer.h"

#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QPointF>
#include <QSize>
#include <QTimer>

#include <deque>
#include <functional>
#include <memory>
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
  // Swap in a new scene without blocking the GUI thread (P07 T2, E-21).
  // `make` runs on the compile thread: DualC's codegen, no GL. It throws
  // std::exception to fail. The program is compiled and linked there, on a
  // context shared with the widget's, while the widget keeps drawing the scene
  // on screen. Back on the GUI thread, `finish` (when given) may edit the
  // scene's binding values, then the program, grids and bindings are taken
  // in one step, and sceneApplied follows as for setScene. A failure emits
  // sceneFailed with the exception's text or the shader log, and the scene on
  // screen stays. Only the last build counts: a later buildScene, setScene or
  // clearScene drops an earlier build's result unseen.
  using SceneMaker = std::function<dce::fieldgraph::GlslScene()>;
  using SceneFinish = std::function<void(dce::fieldgraph::GlslScene&)>;
  void buildScene(SceneMaker make, bool frame, SceneFinish finish = {});
  // A buildScene has not come back yet.
  bool building() const { return building_; }
  // Take the scene off screen: the viewport shows the background, and the
  // camera stays. Links nothing.
  void clearScene();
  bool hasScene() const { return hasScene_; }
  const dce::fieldgraph::GlslScene& scene() const { return scene_; }

  // A parameter edit: set binding `index` of the scene on screen to `value`
  // (as many floats as the binding has) and upload that one uniform. It never
  // recompiles or relinks. The view previews and settles as for a camera move.
  // An edit lasts until the next setScene, which brings its own values.
  void setParameter(int index, std::vector<float> value);

  // Set the trace box of the scene on screen (scene().bounds, and the
  // uBoundsMin/uBoundsMax uniforms): an object moved as uniforms stays inside
  // it. No compile, no link; it lasts until the next scene.
  void setTraceBounds(const dualc::BBox& bounds);

  // The transform gizmo (docs/command_reference/01-viewport.md § Transform
  // gizmo). The window names the object it acts on and that object's pose; id
  // 0 for none. While a target is set, its axes are drawn at its origin.
  void setGizmoTarget(int id, const Pose& pose);
  int gizmoTarget() const { return gizmoId_; }
  // A modal drag, as the keys start it: g moves, r rotates, s scales, with
  // the mouse at `pos`. x, y, z constrain it to a world axis (the same key
  // again frees it); a left click or Return ends it (gizmoFinished, committed),
  // Escape or a right click cancels it. Mouse moves emit gizmoDragged.
  void startGizmo(GizmoMode mode, QPointF pos);
  void setGizmoAxis(int axis);
  void moveGizmo(QPointF pos);
  void endGizmo(bool commit);
  const GizmoDrag* gizmoDrag() const { return gizmo_ ? &*gizmo_ : nullptr; }

  // Highlight object `id` of a lowered scene, 0 for none (kSelectedUniform).
  // It lasts across setScene and repaints at full resolution.
  void setSelectedObject(int id);
  int selectedObject() const { return selected_; }

  // The section planes (TraceRenderer::SectionPlanes): a uniform-only change,
  // repainted at full resolution, kept across setScene.
  void setSection(const TraceRenderer::SectionPlanes& s);
  const TraceRenderer::SectionPlanes& section() const { return renderer_.section(); }

  // Program links and shader compiles attempted so far, and the GL compile +
  // link time of the scene on screen, in ms.
  int linkCount() const { return renderer_.linkCount(); }
  int compileCount() const { return renderer_.compileCount(); }
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
  // A buildScene started on the compile thread.
  void buildStarted();
  // A left click (press and release with no drag) at `pos`, in widget pixels.
  // It moves nothing; the window picks the object under it.
  void clicked(QPointF pos);
  // The gizmo's object `id` is at `pose` during a drag: a preview, as uniforms.
  void gizmoDragged(int id, const Pose& pose);
  // The `mode` drag ended: at `pose` when `committed`, else back at its start.
  void gizmoFinished(int id, const Pose& pose, bool committed, GizmoMode mode);
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
  // The compile thread, its shared context and the build in flight.
  struct Compiler;
  struct Build;
  void startBuild();
  // Take `scene`, already on the renderer, as the scene on screen.
  void takeScene(dce::fieldgraph::GlslScene scene, bool frame);
  void buildDone(int ticket, GLuint program, dce::fieldgraph::GlslScene scene,
                 QString error, int compiles, int links, double ms);
  void stopCompiler();
  void paintGizmo();
  bool moving() const { return drag_ != Drag::None || settle_.isActive(); }

  dce::fieldgraph::GlslScene scene_;  // the scene on screen, once hasScene_
  bool hasScene_ = false;
  // A scene given before GL was up, and whether to frame it.
  std::optional<dce::fieldgraph::GlslScene> pending_;
  bool pendingFrame_ = false;
  std::unique_ptr<Compiler> compiler_;
  std::shared_ptr<Build> build_;  // the last buildScene, until it comes back
  int ticket_ = 0;  // bumped by every scene change; older builds are stale
  int deadTicket_ = 0;  // builds up to this one linked on a context now gone
  bool building_ = false;
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
  QPointF pressPos_;
  bool dragged_ = false;  // the held button moved past the drag distance
  int selected_ = 0;
  int gizmoId_ = 0;
  Pose gizmoPose_;
  std::optional<GizmoDrag> gizmo_;
  Pose gizmoAt_;  // the drag's pose now
  QTimer settle_;  // single-shot; running = inside the settle window
  QTimer gpuPoll_;  // runs while GPU timer queries are in flight
  struct TimedFrame {
    bool preview;
    QSize traced;
  };
  std::deque<TimedFrame> timedFrames_;  // one per query in flight, paint order
};
