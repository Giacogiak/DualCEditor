#pragma once

#include "field_glsl.h"
#include "orbit_camera.h"

#include <QOpenGLFunctions_3_3_Core>

#include <string>
#include <vector>

// Draws DualC's generated sphere-tracer: the program built from
// vertexShaderSource + assembleTraceShader, a full-screen triangle, and every
// uniform kTraceFramework and the scene bindings declare, fed the way
// dualc_field_view feeds them. GL calls go through QOpenGLFunctions_3_3_Core
// (docs/design/01-render-path.md), so the same renderer serves the widget and
// the off-screen --snapshot path. Every call needs the owning context current.
class TraceRenderer : protected QOpenGLFunctions_3_3_Core {
 public:
  // Resolve the GL entry points on the current context. False when it is not
  // an OpenGL 3.3 core context.
  bool initialize();

  // Link the trace program for `scene`, upload its mesh grids and set its
  // bindings. On failure the previous scene stays and `*error` holds the log.
  bool setScene(const dce::fieldgraph::GlslScene& scene, std::string* error);
  bool hasScene() const { return program_ != 0; }

  // Draw one width x height frame into the bound framebuffer.
  void render(int width, int height, const OrbitCamera& cam);

  // Delete the GL objects.
  void release();

 private:
  void releaseTextures();
  void setUniform1f(const char* name, float v);
  void setUniform1i(const char* name, int v);
  void setUniform3(const char* name, const dualc::Vector3& v);
  void setUniform3f(const char* name, float x, float y, float z);
  void setBinding(const dce::fieldgraph::UniformBinding& b);
  GLuint uploadGrid3D(const dce::fieldgraph::MeshTexture& mesh, int unit);

  GLuint program_ = 0;
  GLuint vao_ = 0;
  std::vector<GLuint> textures_;
  dualc::BBox bounds_;
  float stepScale_ = 1.0f;
  float featureScale_ = 0.0f;
};
