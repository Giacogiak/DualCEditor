// A lowered scene on GL, off-screen (QOffscreenSurface + FBO, as --snapshot):
//  1. a scene of one graph, DualC's colour, identity transform, renders the
//     image of the graph alone;
//  2. three objects side by side render each in its own colour;
//  3. hiding an object removes it and showing it again restores the image,
//     with no shader compile and no program link;
//  4. the per-object culling (P07 T1) leaves the images of 1 and 2 as the
//     plain union draws them, and a culling-box upload neither compiles nor
//     links;
//  5. the CPU pick and the culled image agree, pixel by pixel, on which object
//     is seen in a scene where the culling skips the far object.
// Exits 77 (ctest: skipped) with no OpenGL 3.3 core context.
//
// usage: scene_render_test <graph.json>

#include "graph_file.h"
#include "graph_scene.h"
#include "orbit_camera.h"
#include "pick.h"
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
  r.setParameter(c = low.scene.bindings[std::size_t(low.objects[0].colourBinding)]);

  // 4. A culling-box upload is a uniform edit. The last object's box moved
  // off it culls it wherever the others are nearer than the box: the image
  // changes. (The first object is never culled: nothing is nearer yet.)
  fg::UniformBinding boxMin = low.scene.bindings[std::size_t(low.objects[2].boxMinBinding)];
  fg::UniformBinding boxMax = low.scene.bindings[std::size_t(low.objects[2].boxMaxBinding)];
  const fg::UniformBinding boxMin0 = boxMin, boxMax0 = boxMax;
  boxMin.value[1] += 10.0f;
  boxMax.value[1] += 10.0f;
  r.setParameter(boxMin);
  r.setParameter(boxMax);
  check(differing(draw(r, cam), all) > 200, "the culling reads the box uniforms");
  r.setParameter(boxMin0);
  r.setParameter(boxMax0);
  check(differing(draw(r, cam), all) == 0, "the box put back restores the image");
  std::printf("scene_render_test: %d links, %d compiles after the box uploads\n",
              r.linkCount(), r.compileCount());
  check(r.linkCount() == links && r.compileCount() == compiles,
        "a culling-box upload neither compiles nor links");

  // The culled images against the plain union's (lowerScene(scene, false), the
  // P01 shader): within the driver noise of step 1.
  const auto unculled = [&](const Scene& scene, const QImage& culled, const char* what) {
    if (!r.setScene(lowerScene(scene, false).scene, &error)) {
      std::fprintf(stderr, "%s\n", error.c_str());
      return check(false, what);
    }
    int most = 0;
    const long long n = differing(draw(r, cam), culled, &most);
    std::printf("scene_render_test: %s: culled vs plain union %lld px differ, by at most %d\n",
                what, n, most);
    check(n <= kW * kH / 1000 && most <= 1, what);
  };
  unculled(three, all, "three spheres, culled as unculled");
  frameBounds(cam, plain.scene->bounds);
  unculled(one, asScene, "the one-object scene, culled as unculled");

  // 5. Two spheres one behind the other along the view axis: wherever the
  // near one covers the far one, the culling skips the far one. Each pixel's
  // pick names the object whose colour the image shows there.
  Scene stack;
  const SceneColour red = {0.9f, 0.1f, 0.1f}, blue = {0.1f, 0.1f, 0.9f};
  stack.add("near", sphere).colour = red;
  SceneObject& far = stack.add("far", parseGraphText("sphere(radius=1.2)", false));
  far.colour = blue;
  far.transform = dualc::Mat4::translation({0.4, 0.3, -4.0});
  const LoweredScene stackLow = lowerScene(stack);
  if (!r.setScene(stackLow.scene, &error))
    return std::fprintf(stderr, "%s\n", error.c_str()), 1;
  OrbitCamera view;
  view.yaw = 0.0;
  view.pitch = 0.0;
  view.dist = 6.0;
  const QImage seen = draw(r, view);
  const ScenePicker picker(stack, stackLow);
  long long agree = 0, disagree = 0, counts[3] = {0, 0, 0};
  for (int y = 2; y < kH; y += 4)
    for (int x = 2; x < kW; x += 4) {
      const QRgb p = seen.pixel(x, y);
      // The colour of a lit pixel names its object; the background is grey.
      const int shown = qRed(p) > qBlue(p) + 40 ? 1 : qBlue(p) > qRed(p) + 40 ? 2 : 0;
      const int picked = picker.pick(viewRay(view, x + 0.5, y + 0.5, kW, kH)).id;
      ++counts[picked];
      (shown == picked ? agree : disagree)++;
    }
  std::printf("scene_render_test: pick vs culled image: %lld agree, %lld disagree "
              "(picked none %lld, near %lld, far %lld)\n",
              agree, disagree, counts[0], counts[1], counts[2]);
  check(counts[1] > 50 && counts[2] > 50, "both stacked spheres are seen and picked");
  // Pixels on a silhouette may fall either side: allow a few.
  check(disagree <= (agree + disagree) / 200, "the pick and the culled image agree");
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
