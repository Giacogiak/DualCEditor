#include "snapshot.h"

#include "orbit_camera.h"
#include "trace_renderer.h"

#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>

#include <cstdio>
#include <memory>
#include <string>

int renderSnapshot(const dce::fieldgraph::GlslScene& scene, const QString& path,
                   int width, int height) {
  QOpenGLContext context;  // takes QSurfaceFormat::defaultFormat(): 3.3 core
  if (!context.create()) {
    std::fprintf(stderr, "DualCEditor: snapshot: cannot create an OpenGL context\n");
    return kExitNoGl;
  }
  QOffscreenSurface surface;
  surface.setFormat(context.format());
  surface.create();
  if (!surface.isValid() || !context.makeCurrent(&surface)) {
    std::fprintf(stderr, "DualCEditor: snapshot: cannot make the context current\n");
    return kExitNoGl;
  }

  int code = 0;
  {
    TraceRenderer renderer;
    if (!renderer.initialize()) {
      const QSurfaceFormat f = context.format();
      std::fprintf(stderr,
                   "DualCEditor: snapshot: need OpenGL 3.3 core, got %d.%d%s\n",
                   f.majorVersion(), f.minorVersion(),
                   context.isOpenGLES() ? " ES" : "");
      code = kExitNoGl;
    } else {
      std::fprintf(stdout, "DualCEditor: GL renderer: %s\n",
                   reinterpret_cast<const char*>(
                       context.functions()->glGetString(GL_RENDERER)));
      std::string error;
      if (!renderer.setScene(scene, &error)) {
        std::fprintf(stderr, "DualCEditor: snapshot: %s\n", error.c_str());
        code = 1;
      } else {
        auto fbo = std::make_unique<QOpenGLFramebufferObject>(width, height);
        OrbitCamera cam;
        frameBounds(cam, scene.bounds);
        fbo->bind();
        renderer.render(width, height, cam);
        context.functions()->glFinish();
        const QImage image = fbo->toImage();
        fbo->release();
        if (image.isNull() || !image.save(path, "PNG")) {
          std::fprintf(stderr, "DualCEditor: snapshot: cannot write %s\n",
                       qPrintable(path));
          code = 1;
        } else {
          std::fprintf(stdout, "DualCEditor: snapshot -> %s\n", qPrintable(path));
        }
      }
      renderer.release();
    }
  }
  context.doneCurrent();
  return code;
}
