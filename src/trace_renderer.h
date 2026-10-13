#pragma once

#include "field_glsl.h"
#include "orbit_camera.h"

#include <QOpenGLFunctions_3_3_Core>
#include <QSize>

#include <deque>
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

  // Link the trace program for `scene` (assembleSceneShader: a lowered scene
  // gets its per-object colour), upload its mesh grids and set its
  // bindings. On failure the previous scene stays and `*error` holds the log.
  bool setScene(const dce::fieldgraph::GlslScene& scene, std::string* error);
  // The two halves of setScene, for a compile on another context of the same
  // share group (Viewport::buildScene, docs/design/01-render-path.md
  // § Asynchronous compile). linkProgram compiles and links `scene`'s program
  // on whatever context is current through `gl`, adding its attempts to
  // `*compiles` and `*links` and its time to `*ms`; 0 on failure, with the log
  // in `*error`. adoptProgram takes a linked program as the scene on screen:
  // the previous program and grids go, the grids and every binding go up.
  static GLuint linkProgram(QOpenGLFunctions_3_3_Core& gl,
                            const dce::fieldgraph::GlslScene& scene, std::string* error,
                            int* compiles, int* links, double* ms);
  void adoptProgram(GLuint program, const dce::fieldgraph::GlslScene& scene,
                    double compileMs);
  // Count compiles and links made on another context (linkProgram).
  void countElsewhere(int compiles, int links) {
    compileCount_ += compiles;
    linkCount_ += links;
  }
  // Delete a program linked elsewhere and never adopted.
  void dropProgram(GLuint program) { glDeleteProgram(program); }
  bool hasScene() const { return program_ != 0; }
  // Drop the trace program and its mesh grids: render draws the background.
  void clearScene();
  // Program links attempted so far (a parameter edit must not add one), and
  // the wall time of the last successful compile and link, in ms.
  int linkCount() const { return linkCount_; }
  // Shader compiles attempted so far (vertex and fragment: two per setScene).
  int compileCount() const { return compileCount_; }
  double compileMs() const { return compileMs_; }

  // A parameter edit: upload one binding of the scene on screen to its
  // uniform. No compile, no link.
  void setParameter(const dce::fieldgraph::UniformBinding& b);
  // The selected object's id, 0 for none: kSelectedUniform, set at each
  // render (a lowered scene's shader has it; a single graph's ignores it).
  void setSelectedObject(int id) { selected_ = id; }

  // Draw one width x height frame into the bound framebuffer.
  void render(int width, int height, const OrbitCamera& cam);

  // dualc_field_view's adaptive preview: draw at (width x height) * scale into
  // an offscreen RGBA8 target, then blit it up with GL_LINEAR into
  // `targetFbo`, which is left bound. Returns the size actually traced.
  QSize renderPreview(GLuint targetFbo, int width, int height, float scale,
                      const OrbitCamera& cam);

  // Block until the GL work issued so far has finished (frame timing).
  void finish() { glFinish(); }

  // GPU frame timing with GL_TIME_ELAPSED queries, read back without a stall.
  // beginGpuTimer starts a query (false when kGpuTimers are all in flight, and
  // then endGpuTimer must not be called); takeGpuTime returns the oldest
  // finished one, in ms, in the order they were begun.
  static constexpr int kGpuTimers = 4;
  bool beginGpuTimer();
  void endGpuTimer();
  bool takeGpuTime(double* ms);
  bool gpuTimersInFlight() const { return !timersInFlight_.empty(); }

  // Delete the GL objects.
  void release();

 private:
  void releaseTextures();
  void releasePreviewTarget();
  void ensurePreviewTarget(int width, int height);
  void setUniform1f(const char* name, float v);
  void setUniform1i(const char* name, int v);
  void setUniform3(const char* name, const dualc::Vector3& v);
  void setUniform3f(const char* name, float x, float y, float z);
  void setBinding(const dce::fieldgraph::UniformBinding& b);
  GLuint uploadGrid3D(const dce::fieldgraph::MeshTexture& mesh, int unit);

  GLuint program_ = 0;
  GLuint vao_ = 0;
  std::vector<GLuint> textures_;
  GLuint previewFbo_ = 0, previewRbo_ = 0;
  QSize previewSize_;
  std::vector<GLuint> freeTimers_;      // GL_TIME_ELAPSED query objects
  std::deque<GLuint> timersInFlight_;  // begun, result not read yet
  int linkCount_ = 0;
  int compileCount_ = 0;
  double compileMs_ = 0.0;
  dualc::BBox bounds_;
  float stepScale_ = 1.0f;
  float featureScale_ = 0.0f;
  int selected_ = 0;
};
