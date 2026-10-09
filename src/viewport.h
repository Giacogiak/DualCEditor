#pragma once

#include <QOpenGLWidget>
#include <QMouseEvent>
#include <glm/glm.hpp>
#include <memory>

// Forward declare to avoid circular includes
namespace dualc {
  class ImplicitField;
}

/**
 * Viewport: a QOpenGLWidget that hosts the DualC field renderer.
 *
 * Sphere-traces the field SDF in real time, with adaptive preview scale.
 * Left-drag orbits the camera; scroll dollies.
 * Tab selects a parameter; [ / ] nudge it (uniform-only, no recompile).
 *
 * Spike A: Host the field renderer in QOpenGLWidget.
 * Later: Spike B will add object IDs to the raymarcher for picking.
 */
class Viewport : public QOpenGLWidget {
  Q_OBJECT

public:
  explicit Viewport(QWidget *parent = nullptr);
  ~Viewport();

protected:
  void initializeGL() override;
  void paintGL() override;
  void resizeGL(int w, int h) override;

  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void wheelEvent(QWheelEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;

private:
  // Camera
  glm::vec3 m_center;
  float m_radius;
  float m_theta;  // Polar angle
  float m_phi;    // Azimuthal angle

  // Preview state
  float m_previewScale;
  bool m_isDragging;
  QPoint m_lastMousePos;
  QElapsedTimer m_idleTimer;

  // GL resources (will be populated in Spike A)
  GLuint m_shaderProgram;
  GLuint m_vao;

  void updatePreviewScale();
  void resetIdleTimer();
};
