#include "trace_renderer.h"

#include "scene.h"

#include <QElapsedTimer>
#include <QOpenGLContext>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fg = dce::fieldgraph;
using dualc::Vector3;

namespace {

GLuint compileStage(QOpenGLFunctions_3_3_Core& gl, GLenum kind,
                    const std::string& src, const char* label, std::string* error) {
  const GLuint s = gl.glCreateShader(kind);
  const char* csrc = src.c_str();
  gl.glShaderSource(s, 1, &csrc, nullptr);
  gl.glCompileShader(s);
  GLint ok = 0;
  gl.glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[8192] = {};
    gl.glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    *error = std::string(label) + " shader compile error:\n" + log;
    gl.glDeleteShader(s);
    return 0;
  }
  return s;
}

}  // namespace

bool TraceRenderer::initialize() {
  const QOpenGLContext* ctx = QOpenGLContext::currentContext();
  if (!ctx || ctx->isOpenGLES() || ctx->format().version() < qMakePair(3, 3))
    return false;
  if (!initializeOpenGLFunctions()) return false;
  if (!vao_) glGenVertexArrays(1, &vao_);
  if (freeTimers_.empty() && timersInFlight_.empty()) {
    freeTimers_.resize(kGpuTimers);
    glGenQueries(kGpuTimers, freeTimers_.data());
  }
  return true;
}

bool TraceRenderer::setScene(const fg::GlslScene& scene, std::string* error) {
  // Compile and link, timed together. Reading the link status waits for the
  // driver to finish the link.
  std::string fragment;
  try {
    fragment = assembleSceneShader(scene);
  } catch (const std::runtime_error& e) {
    *error = e.what();
    return false;
  }
  QElapsedTimer clock;
  clock.start();
  ++compileCount_;
  const GLuint vs = compileStage(*this, GL_VERTEX_SHADER,
                                 fg::vertexShaderSource(false), "vertex", error);
  if (!vs) return false;
  ++compileCount_;
  const GLuint fs = compileStage(*this, GL_FRAGMENT_SHADER, fragment, "fragment", error);
  if (!fs) {
    glDeleteShader(vs);
    return false;
  }
  const GLuint p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glLinkProgram(p);
  ++linkCount_;
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[8192] = {};
    glGetProgramInfoLog(p, sizeof(log), nullptr, log);
    *error = std::string("program link error:\n") + log;
    glDeleteProgram(p);
    return false;
  }
  compileMs_ = clock.nsecsElapsed() * 1e-6;

  if (program_) glDeleteProgram(program_);
  program_ = p;
  bounds_ = scene.bounds;
  stepScale_ = scene.stepScale;
  featureScale_ = scene.featureScale;

  // The previous scene's grids go first: a reload re-bakes every mesh.
  glUseProgram(program_);
  releaseTextures();
  for (std::size_t i = 0; i < scene.meshes.size(); ++i)
    textures_.push_back(uploadGrid3D(scene.meshes[i], static_cast<int>(i)));
  for (const fg::UniformBinding& b : scene.bindings) setBinding(b);
  return true;
}

void TraceRenderer::render(int width, int height, const OrbitCamera& cam) {
  glViewport(0, 0, width, height);
  glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  if (!program_) return;

  // Uniforms as dualc_field_view's drawFrame sets them, section planes off.
  glUseProgram(program_);
  const CameraFrame c = cameraFrame(cam);
  setUniform3("uCamPos", c.pos);
  setUniform3("uCamForward", c.forward);
  setUniform3("uCamRight", c.right);
  setUniform3("uCamUp", c.up);
  setUniform1f("uTanHalfFov", float(kTanHalfFov));
  setUniform1f("uAspect", float(width) / float(std::max(height, 1)));
  setUniform3("uBoundsMin", bounds_.min);
  setUniform3("uBoundsMax", bounds_.max);
  setUniform3("uLightDir", Vector3{0.5, 0.85, 0.6}.normalize());

  // March constants scale to the finest feature (a TPMS wavelength), else to
  // the bounds diagonal. A stepScale of 1 means a true SDF: plain sphere steps.
  const Vector3 e = bounds_.extent();
  const double diag = std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z);
  const double feat =
      featureScale_ > 0.0f ? std::min(double(featureScale_), diag) : diag;
  const float stepMin = float(std::min(diag * 0.002, feat * 0.003));
  setUniform1f("uStepScale", stepScale_);
  setUniform1f("uMetricSDF", stepScale_ >= 0.999f ? 1.0f : 0.0f);
  setUniform1f("uStepMin", stepMin);
  setUniform1f("uStepMax", float(std::min(diag * 0.100, feat * 0.350)));
  setUniform1f("uHmem", stepMin);  // FD normals use the smallest step

  const float noClip[12] = {};
  glUniform4fv(glGetUniformLocation(program_, "uClip"), 3, noClip);
  setUniform1i("uClipMask", 0);
  setUniform1f(kSelectedUniform, float(selected_));

  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
}

QSize TraceRenderer::renderPreview(GLuint targetFbo, int width, int height,
                                   float scale, const OrbitCamera& cam) {
  const int sw = std::max(1, int(float(width) * scale));
  const int sh = std::max(1, int(float(height) * scale));
  ensurePreviewTarget(sw, sh);
  glBindFramebuffer(GL_FRAMEBUFFER, previewFbo_);
  render(sw, sh, cam);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, previewFbo_);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetFbo);
  glBlitFramebuffer(0, 0, sw, sh, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
  glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
  glViewport(0, 0, width, height);
  return QSize(sw, sh);
}

// A plain RGBA8 renderbuffer, as in dualc_field_view: it is only ever blitted,
// never sampled.
void TraceRenderer::ensurePreviewTarget(int width, int height) {
  if (previewFbo_ && previewSize_ == QSize(width, height)) return;
  if (!previewFbo_) glGenFramebuffers(1, &previewFbo_);
  if (!previewRbo_) glGenRenderbuffers(1, &previewRbo_);
  glBindRenderbuffer(GL_RENDERBUFFER, previewRbo_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, previewFbo_);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                            previewRbo_);
  previewSize_ = QSize(width, height);
}

void TraceRenderer::setParameter(const fg::UniformBinding& b) {
  if (!program_) return;
  glUseProgram(program_);
  setBinding(b);
}

bool TraceRenderer::beginGpuTimer() {
  if (freeTimers_.empty()) return false;
  const GLuint q = freeTimers_.back();
  freeTimers_.pop_back();
  timersInFlight_.push_back(q);
  glBeginQuery(GL_TIME_ELAPSED, q);
  return true;
}

void TraceRenderer::endGpuTimer() {
  glEndQuery(GL_TIME_ELAPSED);
}

bool TraceRenderer::takeGpuTime(double* ms) {
  if (timersInFlight_.empty()) return false;
  const GLuint q = timersInFlight_.front();
  GLint available = 0;
  glGetQueryObjectiv(q, GL_QUERY_RESULT_AVAILABLE, &available);
  if (!available) return false;
  GLuint64 ns = 0;
  glGetQueryObjectui64v(q, GL_QUERY_RESULT, &ns);
  timersInFlight_.pop_front();
  freeTimers_.push_back(q);
  *ms = double(ns) * 1e-6;
  return true;
}

void TraceRenderer::releasePreviewTarget() {
  if (previewRbo_) glDeleteRenderbuffers(1, &previewRbo_);
  if (previewFbo_) glDeleteFramebuffers(1, &previewFbo_);
  previewFbo_ = previewRbo_ = 0;
  previewSize_ = QSize();
}

void TraceRenderer::release() {
  releaseTextures();
  releasePreviewTarget();
  for (GLuint q : timersInFlight_) freeTimers_.push_back(q);
  timersInFlight_.clear();
  if (!freeTimers_.empty())
    glDeleteQueries(static_cast<GLsizei>(freeTimers_.size()), freeTimers_.data());
  freeTimers_.clear();
  if (program_) glDeleteProgram(program_);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  program_ = 0;
  vao_ = 0;
}

void TraceRenderer::clearScene() {
  releaseTextures();
  if (program_) glDeleteProgram(program_);
  program_ = 0;
}

void TraceRenderer::releaseTextures() {
  if (textures_.empty()) return;
  glDeleteTextures(static_cast<GLsizei>(textures_.size()), textures_.data());
  textures_.clear();
}

void TraceRenderer::setUniform1f(const char* name, float v) {
  glUniform1f(glGetUniformLocation(program_, name), v);
}
void TraceRenderer::setUniform1i(const char* name, int v) {
  glUniform1i(glGetUniformLocation(program_, name), v);
}
void TraceRenderer::setUniform3(const char* name, const Vector3& v) {
  glUniform3f(glGetUniformLocation(program_, name), float(v.x), float(v.y),
              float(v.z));
}
void TraceRenderer::setUniform3f(const char* name, float x, float y, float z) {
  glUniform3f(glGetUniformLocation(program_, name), x, y, z);
}

// A parameter edit: no recompile. mat4 goes up row-major (transposed) to match
// Mat4::transformPoint, as in dualc_field_view.
void TraceRenderer::setBinding(const fg::UniformBinding& b) {
  const GLint loc = glGetUniformLocation(program_, b.name.c_str());
  if (loc < 0) return;
  if (b.glslType == "float") glUniform1f(loc, b.value[0]);
  else if (b.glslType == "vec3") glUniform3f(loc, b.value[0], b.value[1], b.value[2]);
  else if (b.glslType == "mat4") glUniformMatrix4fv(loc, 1, GL_TRUE, b.value.data());
}

// raymarch_gl's uploadGrid3D: one baked grid to a GL_R32F 3D texture on `unit`,
// plus the <base>_texMin/_texScale/_texDim uniforms the codegen names off the
// sampler ("u_n5_tex" -> "u_n5").
GLuint TraceRenderer::uploadGrid3D(const fg::MeshTexture& mesh, int unit) {
  const dualc::Vector3i& res = mesh.resolution;
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_3D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, res.x, res.y, res.z, 0, GL_RED, GL_FLOAT,
               mesh.values.data());

  const std::string base = mesh.samplerName.substr(0, mesh.samplerName.size() - 4);
  const Vector3 ext = mesh.region.extent();
  setUniform1i(mesh.samplerName.c_str(), unit);
  setUniform3((base + "_texMin").c_str(), mesh.region.min);
  setUniform3f((base + "_texScale").c_str(),
               float((res.x - 1) / std::max(ext.x, 1e-12)),
               float((res.y - 1) / std::max(ext.y, 1e-12)),
               float((res.z - 1) / std::max(ext.z, 1e-12)));
  setUniform3f((base + "_texDim").c_str(), float(res.x), float(res.y),
               float(res.z));
  return tex;
}
