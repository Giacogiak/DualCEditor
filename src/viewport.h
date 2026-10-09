#pragma once

#include "field_glsl.h"
#include "orbit_camera.h"
#include "trace_renderer.h"

#include <QOpenGLWidget>

// The 3D viewport: a GL 3.3 core QOpenGLWidget that sphere-traces a compiled
// field graph through DualC's generated shader (TraceRenderer), on the framed
// default view of DualC's viewers. Camera input lands in plan/10_P00 T2.
class Viewport : public QOpenGLWidget {
  Q_OBJECT

 public:
  explicit Viewport(dce::fieldgraph::GlslScene scene, QWidget* parent = nullptr);
  ~Viewport() override;

 protected:
  void initializeGL() override;
  void paintGL() override;

 private:
  void releaseGL();

  dce::fieldgraph::GlslScene scene_;
  OrbitCamera camera_;
  TraceRenderer renderer_;
  bool glReady_ = false;
};
