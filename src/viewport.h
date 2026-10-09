#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>

// The 3D viewport: a GL 3.3 core QOpenGLWidget that will host DualC's
// sphere-tracer (plan/10_P00). For now it brings the context up, reports the
// GL renderer and clears.
class Viewport : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
  Q_OBJECT

 public:
  explicit Viewport(QWidget* parent = nullptr);

 protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;
};
