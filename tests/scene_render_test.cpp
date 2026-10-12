// A lowered scene on GL, off-screen (QOffscreenSurface + FBO, as --snapshot):
//  1. a scene of one graph, DualC's colour, identity transform, renders the
//     image of the graph alone;
//  2. three objects side by side render each in its own colour;
//  3. hiding an object removes it and showing it again restores the image,
//     with no shader compile and no program link.
// Exits 77 (ctest: skipped) with no OpenGL 3.3 core context.
//
// usage: scene_render_test <graph.json>

#include "graph_file.h"
#include "graph_scene.h"
#include "orbit_camera.h"
#include "scene.h"
#include "trace_renderer.h"

#include <QGuiApplication>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QSurfaceFormat>

#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>

namespace fg = dce::fieldgraph;

namespace {

constexpr int kW = 400, kH = 300;
int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "scene_render_test: FAIL %s\n", what);
    ++failures;
  }
}

QImage draw(TraceRenderer& r, const OrbitCamera& cam) {
  QOpenGLFramebufferObject fbo(kW, kH);
  fbo.bind();
  r.render(kW, kH, cam);
  r.finish();
  QImage image = fbo.toImage().convertToFormat(QImage::Format_RGB32);
  fbo.release();
  return image;
}

long long differing(const QImage& a, const QImage& b, int* maxDiff = nullptr) {
  long long n = 0;
  int worst = 0;
  for (int y = 0; y < kH; ++y)
    for (int x = 0; x < kW; ++x) {
      const QRgb p = a.pixel(x, y), q = b.pixel(x, y);
      if (p == q) continue;
      ++n;
      worst = std::max({worst, std::abs(qRed(p) - qRed(q)), std::abs(qGreen(p) - qGreen(q)),
                        std::abs(qBlue(p) - qBlue(q))});
    }
  if (maxDiff) *maxDiff = worst;
  return n;
}

// The mean colour of the pixels where `with` differs from `without`: the
// pixels an object covers, read off the image that shows it.
bool meanWhereDiffers(const QImage& with, const QImage& without, double rgb[3]) {
  double sum[3] = {0, 0, 0};
  long long n = 0;
  for (int y = 0; y < kH; ++y)
    for (int x = 0; x < kW; ++x) {
      const QRgb p = with.pixel(x, y);
      if (p == without.pixel(x, y)) continue;
      sum[0] += qRed(p);
      sum[1] += qGreen(p);
      sum[2] += qBlue(p);
      ++n;
    }
  if (n < 200) return false;
  for (int i = 0; i < 3; ++i) rgb[i] = sum[i] / double(n);
  return true;
}

void setVisible(TraceRenderer& r, const LoweredScene& low, int object, bool on) {
  fg::UniformBinding b = low.scene.bindings[std::size_t(low.objects[std::size_t(object)].visibleBinding)];
  b.value = {on ? 1.0f : 0.0f};
  r.setParameter(b);
}

int run(const QString& graphPath) {
  TraceRenderer r;
  if (!r.initialize()) {
    std::printf("scene_render_test: skipped, no OpenGL 3.3 core context\n");
    return 77;
  }
  std::string error;

  // 1. One graph alone against the same graph as a one-object scene.
  GraphLoad plain = loadGraphFile(graphPath);
  GraphParse parse = parseGraphFile(graphPath);
  if (!plain.scene || !parse.graph) {
    std::fprintf(stderr, "scene_render_test: %s\n", qPrintable(plain.error));
    return 1;
  }
  Scene one;
  one.add("graph", *parse.graph, graphMeshDir(graphPath));
  const LoweredScene oneLow = lowerScene(one);
  OrbitCamera cam;
  frameBounds(cam, plain.scene->bounds);
  if (!r.setScene(*plain.scene, &error)) return std::fprintf(stderr, "%s\n", error.c_str()), 1;
  const QImage alone = draw(r, cam);
  if (!r.setScene(oneLow.scene, &error)) return std::fprintf(stderr, "%s\n", error.c_str()), 1;
  const QImage asScene = draw(r, cam);
  // Not bit for bit: the program differs (the transform wrapper, the union and
  // sceneColor), and a driver may order the arithmetic differently. On NV92
  // 16 of 120 000 px differ, by 1 level; on llvmpipe none. A wrong lowering
  // (bounds, step sizes, colour) changes far more.
  int worst = 0;
  const long long d1 = differing(alone, asScene, &worst);
  std::printf("scene_render_test: one-object scene vs graph alone: %lld px differ, by at "
              "most %d\n", d1, worst);
  check(d1 <= kW * kH / 1000, "a one-object scene renders the graph's own image");

  // 2. Three spheres along x: red, green, blue.
  Scene three;
  const fg::GraphNode sphere = parseGraphText("sphere(radius=0.7)", false);
  const SceneColour colours[3] = {{0.9f, 0.1f, 0.1f}, {0.1f, 0.9f, 0.1f}, {0.1f, 0.1f, 0.9f}};
  for (int i = 0; i < 3; ++i) {
    SceneObject& o = three.add(QString("sphere %1").arg(i + 1), sphere);
    o.transform = dualc::Mat4::translation({2.0 * (i - 1), 0.0, 0.0});
    o.colour = colours[i];
  }
  const LoweredScene low = lowerScene(three);
  if (!r.setScene(low.scene, &error)) return std::fprintf(stderr, "%s\n", error.c_str()), 1;
  const int links = r.linkCount(), compiles = r.compileCount();
  frameBounds(cam, low.scene.bounds);
  const QImage all = draw(r, cam);

  // 3. Hide each object in turn: what changes is that object, in its colour.
  for (int i = 0; i < 3; ++i) {
    setVisible(r, low, i, false);
    const QImage hidden = draw(r, cam);
    double rgb[3];
    const bool covered = meanWhereDiffers(all, hidden, rgb);
    std::printf("scene_render_test: object %d mean colour (%.0f, %.0f, %.0f)\n", i + 1,
                covered ? rgb[0] : -1.0, covered ? rgb[1] : -1.0, covered ? rgb[2] : -1.0);
    check(covered, "hiding an object removes it from the image");
    check(covered && rgb[i] > rgb[(i + 1) % 3] + 40 && rgb[i] > rgb[(i + 2) % 3] + 40,
          "an object renders in its own colour");
    setVisible(r, low, i, true);
    check(differing(draw(r, cam), all) == 0, "showing it again restores the image");
  }
  // A recolour is a uniform too.
  fg::UniformBinding c = low.scene.bindings[std::size_t(low.objects[0].colourBinding)];
  c.value = {0.1f, 0.1f, 0.9f};
  r.setParameter(c);
  check(differing(draw(r, cam), all) > 200, "a colour edit changes the image");
  std::printf("scene_render_test: %d links, %d compiles after the toggles (%d, %d before)\n",
              r.linkCount(), r.compileCount(), links, compiles);
  check(r.linkCount() == links && r.compileCount() == compiles,
        "visibility and colour edits neither compile nor link");
  r.release();
  return failures ? 1 : 0;
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef Q_OS_LINUX
  if (qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") &&
      qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
  QSurfaceFormat format;
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(format);
  QGuiApplication app(argc, argv);
  if (argc != 2) {
    std::fprintf(stderr, "usage: scene_render_test <graph.json>\n");
    return 1;
  }
  QOpenGLContext context;
  QOffscreenSurface surface;
  if (!context.create()) {
    std::printf("scene_render_test: skipped, no OpenGL context\n");
    return 77;
  }
  surface.setFormat(context.format());
  surface.create();
  if (!surface.isValid() || !context.makeCurrent(&surface)) {
    std::printf("scene_render_test: skipped, cannot make the context current\n");
    return 77;
  }
  const int code = run(QString::fromLocal8Bit(argv[1]));
  context.doneCurrent();
  return code;
}
