#include "viewport.h"
#include <QElapsedTimer>
#include <QWheelEvent>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Viewport::Viewport(QWidget *parent)
    : QOpenGLWidget(parent),
      m_center(0.0f, 0.0f, 0.0f),
      m_radius(2.0f),
      m_theta(glm::pi<float>() / 4.0f),
      m_phi(0.0f),
      m_previewScale(1.0f),
      m_isDragging(false),
      m_shaderProgram(0),
      m_vao(0) {
  setFocusPolicy(Qt::StrongFocus);
}

Viewport::~Viewport() {
}

void Viewport::initializeGL() {
  // Initialize OpenGL 3.3 core context
  // TODO: Load glad, compile a minimal sphere-trace shader
  // Spike A: adapt raymarch_gl.cpp from DualC examples/
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
}

void Viewport::paintGL() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // TODO: Sphere-trace and render
  // For now, just render a clear frame
  updatePreviewScale();
}

void Viewport::resizeGL(int w, int h) {
  glViewport(0, 0, w, h);
}

void Viewport::mousePressEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton) {
    m_isDragging = true;
    m_lastMousePos = e->pos();
  }
}

void Viewport::mouseMoveEvent(QMouseEvent *e) {
  if (m_isDragging) {
    QPoint delta = e->pos() - m_lastMousePos;
    m_lastMousePos = e->pos();

    // Orbit camera
    m_phi += delta.x() * 0.01f;
    m_theta += delta.y() * 0.01f;
    m_theta = glm::clamp(m_theta, 0.01f, glm::pi<float>() - 0.01f);

    m_previewScale = 0.5f;  // Lower res while dragging
    resetIdleTimer();
    update();
  }
}

void Viewport::mouseReleaseEvent(QMouseEvent *e) {
  if (e->button() == Qt::LeftButton) {
    m_isDragging = false;
  }
}

void Viewport::wheelEvent(QWheelEvent *e) {
  float delta = e->angleDelta().y() > 0 ? 0.9f : 1.1f;
  m_radius *= delta;
  m_radius = glm::clamp(m_radius, 0.1f, 100.0f);
  resetIdleTimer();
  update();
}

void Viewport::keyPressEvent(QKeyEvent *e) {
  if (e->key() == Qt::Key_Tab) {
    // TODO: Cycle through parameters for editing
  }
}

void Viewport::updatePreviewScale() {
  // Adaptive preview: low-res while moving, full when idle
  if (m_idleTimer.elapsed() > 180) {  // 180 ms settle
    if (m_previewScale < 1.0f) {
      m_previewScale = 1.0f;
    }
  } else {
    m_previewScale = 0.5f;
  }
}

void Viewport::resetIdleTimer() {
  m_idleTimer.start();
}
