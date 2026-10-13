// Booleans and smooth booleans (P05 T1), GL-free. Over a sphere, a moved and
// scaled box, an imported open box (a `winding` object), a clipped raw gyroid
// and a clipped octet lattice, each of union, intersection, difference, xor
// and the three smooth ones, applied to selections of two and three objects in
// selection order:
//  1. gives an object whose field, read back as dualc_field reads a graph
//     (dumpJson, then parseJson) and built by DualC, equals at sample points
//     the field of the graph written by hand in dualc_field's shorthand: the
//     operands posed, folded left over three operands, and the non-metric
//     ones under `normalize` for a smooth boolean (E-10);
//  2. hides its operands, and undo restores them: the scene equals the one
//     before, and redo brings the same object back;
//  3. a sharp union, intersection and difference are min, max and max(a, -b)
//     of the operands' own fields;
//  4. the results lower to GLSL (normalize over winding and TPMS included).
// And fills (P05 T2): a gyroid, a schwarz-p, an octet and a tapered bcc fill of
// the sphere, the posed box and the winding box, with and without a skin:
//  5. the defaults are the box diagonal / 10, its centre, t a fifth and the
//     radii a tenth of the wavelength;
//  6. the fill's field, read back as dualc_field reads it, equals the one of
//     intersection(onion(normalize(tpms)), volume) or intersection(lattice,
//     volume) written by hand, in a union with onion(offset(volume, -s / 2),
//     s / 2) for a skin s, the winding under normalize there;
//  7. the volume is hidden, undo restores it, and a boolean on the fill works;
//  8. the skin is the volume's inside within s of its surface.
// And the operation catalogue the panels are generated from (P05 T3):
//  9. it holds every boolean DualC parses, every TPMS of tpmsKinds() and every
//     crystal of strutKinds(), in DualC's order, a smooth boolean with k and a
//     sharp one with no parameter;
// 10. every parameter a panel offers is one DualC's builder reads: raising it
//     changes the built field;
// 11. the command-reference page (argv[1], when given) names every operation.

#include "example_common.h"
#include "graph_file.h"
#include "mesh_import.h"
#include "mesh_solid.h"
#include "operation_catalogue.h"
#include "scene.h"
#include "scene_document.h"

#include <QCoreApplication>
#include <QFile>
#include <QUndoStack>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace fg = dce::fieldgraph;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
  std::fprintf(ok ? stdout : stderr, "operations_test: %s %s\n", ok ? "ok  " : "FAIL",
               what.c_str());
  if (!ok) ++failures;
}

// An open box, ±8 mm, its top missing: DualC finds it open, so it imports as
// a `winding` object.
MeshBuffers openBox() {
  MeshBuffers m;
  const float s = 8.0f;
  m.vertices = {-s, -s, -s, s, -s, -s, s, s, -s, -s, s, -s,
                -s, -s, s,  s, -s, s,  s, s, s,  -s, s, s};
  m.indices = {0, 2, 1, 0, 3, 2,   // bottom
               0, 1, 5, 0, 5, 4,   // front
               1, 2, 6, 1, 6, 5,   // right
               2, 3, 7, 2, 7, 6,   // back
               3, 0, 4, 3, 4, 7};  // left
  return m;
}

std::string number(double v) {
  char text[32];
  std::snprintf(text, sizeof text, "%.17g", v);
  return text;
}

// An operand as written by hand: its own shorthand, under scale and transform.
struct Operand {
  int id = 0;
  std::string expr;
  bool metric = true;
  double scale = 1.0;
  dualc::Mat4 transform = dualc::Mat4::identity();

  std::string posed(bool smooth) const {
    std::string e = expr;
    if (scale != 1.0) e = "scale(" + e + ",s=" + number(scale) + ")";
    bool identity = true;
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c) identity = identity && transform.m[r][c] == (r == c ? 1 : 0);
    if (!identity) {
      std::string m;
      for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) m += (m.empty() ? "" : ",") + number(transform.m[r][c]);
      e = "transform(" + e + ",matrix=[" + m + "])";
    }
    return smooth && !metric ? "normalize(" + e + ")" : e;
  }
};

std::vector<dualc::Vector3> samplePoints() {
  std::vector<dualc::Vector3> points;
  const double at[] = {-12.5, -6.25, -0.75, 3.5, 9.25, 14.0};
  for (double x : at)
    for (double y : at)
      for (double z : at) points.push_back({x, y, z * 0.9 + 0.3});
  return points;
}

std::vector<double> values(const fg::GraphNode& graph, const std::vector<dualc::Vector3>& points) {
  GraphDirMeshResolver meshes{QString()};
  const fg::FieldGraph f = fg::FieldGraph::build(graph, meshes);
  std::vector<double> out;
  for (const dualc::Vector3& p : points) out.push_back(f.field().valueAt(p));
  return out;
}

double maxDiff(const std::vector<double>& a, const std::vector<double>& b) {
  double d = a.size() == b.size() ? 0.0 : INFINITY;
  for (std::size_t i = 0; i < a.size() && i < b.size(); ++i) d = std::max(d, std::abs(a[i] - b[i]));
  return d;
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  SceneDocument doc;
  const std::vector<dualc::Vector3> points = samplePoints();

  Operand ball{0, "sphere(radius=10)"};
  Operand box{0, "box(min=[-5,-5,-5],max=[5,5,5])"};
  box.scale = 1.5;
  box.transform.m[0][3] = 6.0;
  box.transform.m[1][3] = -2.0;
  const MeshImport soup = importMeshBuffers(openBox());
  check(soup.graph && meshObjectKind(*soup.graph) == "winding", "the open box imports as winding");
  if (!soup.graph) return 1;
  Operand winding{0, "winding(id=\"" + soup.id + "\")", false};
  Operand gyroid{0, "intersection(box(min=[-10,-10,-10],max=[10,10,10]),gyroid(wavelength=5))",
                 false};
  Operand octet{0,
                "intersection(box(min=[-10,-10,-10],max=[10,10,10]),"
                "octet(wavelength=5,radius=0.6))"};

  for (Operand* o : {&ball, &box, &gyroid, &octet}) {
    o->id = doc.addObject(QString::fromStdString(o->expr.substr(0, o->expr.find('('))),
                          fg::parseShorthand(o->expr));
  }
  winding.id = doc.addObject("soup", *soup.graph);
  doc.setPose(box.id, box.transform, box.scale);

  check(isMetricGraph(fg::parseShorthand(ball.expr)) && isMetricGraph(fg::parseShorthand(octet.expr)),
        "a sphere and a clipped octet lattice are metric");
  check(!isMetricGraph(*soup.graph) && !isMetricGraph(fg::parseShorthand(gyroid.expr)),
        "a winding field and a clipped raw gyroid are not");
  check(isMetricGraph(fg::parseShorthand("onion(normalize(gyroid(wavelength=5)),thickness=1)")),
        "a gyroid under normalize is");

  const std::vector<std::string> ops = {"union",        "intersection",        "difference",
                                        "xor",          "smooth-union",        "smooth-intersection",
                                        "smooth-difference"};
  const std::vector<std::vector<const Operand*>> selections = {
      {&ball, &box}, {&box, &ball}, {&ball, &winding}, {&octet, &gyroid}, {&ball, &octet, &box}};
  const double k = 2.0;

  for (const std::string& op : ops) {
    const bool smooth = isSmoothBooleanOp(op);
    check(isBooleanOp(op), op + " is a DualC boolean");
    std::string token = op;
    std::replace(token.begin(), token.end(), '-', '_');
    std::map<std::string, fg::ParamValue> params;
    if (smooth) params["k"].num = k;
    for (const std::vector<const Operand*>& selection : selections) {
      std::vector<int> ids;
      std::string names;
      for (const Operand* o : selection) {
        ids.push_back(o->id);
        names += (names.empty() ? "" : ", ") + doc.scene().find(o->id)->name.toStdString();
      }
      const std::string what = op + "(" + names + ")";
      const Scene before = doc.scene();
      const int id = doc.applyOperation(op, params, ids);
      const SceneObject* r = doc.scene().find(id);
      check(r && r->operands == ids, what + ": a new object over the operands in selection order");
      if (!r) continue;

      // 1. Against the graph written by hand, read as dualc_field reads it.
      std::string expected = selection[0]->posed(smooth);
      for (std::size_t i = 1; i < selection.size(); ++i)
        expected = token + "(" + expected + "," + selection[i]->posed(smooth) +
                   (smooth ? ",k=" + number(k) : "") + ")";
      const fg::GraphNode read = fg::parseJson(fg::dumpJson(r->graph));
      const double d = maxDiff(values(read, points), values(fg::parseShorthand(expected), points));
      check(d <= 1e-12, what + ": the field matches DualC's on " + expected + " (max diff " +
                            number(d) + ", " + std::to_string(points.size()) + " points)");

      // E-10: the step names the operands wrapped in normalize.
      bool wraps = false;
      for (const Operand* o : selection) wraps = wraps || (smooth && !o->metric);
      const QString text = doc.undoStack()->text(doc.undoStack()->index() - 1);
      check(text.contains("normalize on") == wraps,
            what + ": the step reads \"" + text.toStdString() + "\"");

      // 2. Hidden operands; undo restores them.
      bool hidden = true;
      for (int o : ids) hidden = hidden && !doc.scene().find(o)->visible;
      check(hidden, what + ": the operands are hidden");
      const fg::GraphNode graph = r->graph;
      doc.undoStack()->undo();
      check(sameScene(doc.scene(), before), what + ": undo restores the operands");
      doc.undoStack()->redo();
      const SceneObject* again = doc.scene().find(id);
      check(again && sameGraph(again->graph, graph), what + ": redo brings it back");

      // 3. Sharp booleans against the operands' own fields, two operands.
      if (selection.size() == 2 && !smooth && op != "xor") {
        const std::vector<double> a = values(fg::parseShorthand(selection[0]->posed(false)), points);
        const std::vector<double> b = values(fg::parseShorthand(selection[1]->posed(false)), points);
        std::vector<double> want;
        for (std::size_t i = 0; i < a.size(); ++i)
          want.push_back(op == "union"          ? std::min(a[i], b[i])
                         : op == "intersection" ? std::max(a[i], b[i])
                                                : std::max(a[i], -b[i]));
        check(maxDiff(values(read, points), want) <= 1e-12,
              what + ": min, max or max(a, -b) of the operands' fields");
      }
      // Back to the operands for the next selection.
      doc.undoStack()->undo();
    }
  }

  // 5. Defaults, from a ±10 mm box: diagonal 34.64 mm.
  {
    const FillSpec d = defaultFill("octet", {{-10, -10, -10}, {10, 10, 10}});
    check(d.wavelength == 3.46 && d.radius == 0.346 && d.nodeRadius == 0.346 &&
              std::abs(d.center.x) + std::abs(d.center.y) + std::abs(d.center.z) == 0.0 &&
              d.skin == 0.0,
          "the lattice defaults: wavelength 3.46 mm (diagonal / 10), radii 0.346 mm, at the "
          "centre, no skin");
    const FillSpec g = defaultFill("gyroid", {{0, 0, 0}, {20, 20, 20}});
    check(g.wavelength == 3.46 && g.thickness == 0.692 && g.center.x == 10.0,
          "the TPMS defaults: wall t 0.692 mm (wavelength / 5), centred on the box");
    check(isTpmsKind("gyroid") && isStrutKind("octet") && !isTpmsKind("octet") &&
              !isStrutKind("sphere"),
          "the kinds are DualC's");
  }
  {
    FillSpec bad;
    bad.kind = "sphere";
    check(doc.applyFill(bad, ball.id) == 0 && doc.applyFill(FillSpec{}, 9999) == 0,
          "a fill of an unknown kind or volume is refused");
  }

  // 6, 7. Fills against the graphs written by hand.
  struct Fill {
    FillSpec spec;
    std::string source;  // its shorthand, in world coordinates
  };
  std::vector<Fill> fills;
  {
    FillSpec f;
    f.kind = "gyroid";
    f.wavelength = 5.0;
    f.center = {1.0, -0.5, 0.25};
    f.thickness = 0.75;
    fills.push_back({f, "onion(normalize(gyroid(center=[1,-0.5,0.25],wavelength=5)),thickness=0.75)"});
    f.kind = "schwarz-p";
    fills.push_back({f, "onion(normalize(schwarz_p(center=[1,-0.5,0.25],wavelength=5)),thickness=0.75)"});
    f.kind = "octet";
    f.center = {0, 0, 0};
    f.wavelength = 4.0;
    f.radius = f.nodeRadius = 0.5;
    fills.push_back({f, "octet(center=[0,0,0],wavelength=4,radius=0.5,nodeRadius=0.5)"});
    f.kind = "bcc";
    f.nodeRadius = 0.9;
    fills.push_back({f, "bcc(center=[0,0,0],wavelength=4,radius=0.5,nodeRadius=0.9)"});
  }
  for (const Operand* volume : {&ball, &box, &winding}) {
    for (const Fill& fill : fills) {
      for (const double skin : {0.0, 2.0}) {
        FillSpec spec = fill.spec;
        spec.skin = skin;
        const std::string what = spec.kind + " fill of " +
                                 doc.scene().find(volume->id)->name.toStdString() +
                                 (skin > 0.0 ? ", skin 2 mm" : "");
        const Scene before = doc.scene();
        const int id = doc.applyFill(spec, volume->id);
        const SceneObject* r = doc.scene().find(id);
        check(r && r->operands == std::vector<int>{volume->id} &&
                  !doc.scene().find(volume->id)->visible,
              what + ": a new object over the volume, which is hidden");
        if (!r) continue;
        const std::string posed = volume->posed(false);
        std::string expected = "intersection(" + fill.source + "," + posed + ")";
        if (skin > 0.0) {
          const std::string metric = volume->metric ? posed : "normalize(" + posed + ")";
          expected = "union(" + expected + ",onion(offset(" + metric + ",r=-1),thickness=1))";
        }
        const fg::GraphNode read = fg::parseJson(fg::dumpJson(r->graph));
        const std::vector<double> got = values(read, points);
        const double d = maxDiff(got, values(fg::parseShorthand(expected), points));
        check(d <= 1e-12, what + ": the field matches DualC's on " + expected + " (max diff " +
                              number(d) + ")");
        check(isMetricGraph(r->graph) == volume->metric,
              what + ": metric exactly when the volume is");
        const fg::GraphNode graph = r->graph;
        // A boolean on the filled result: the fill less a sphere.
        const int cut = doc.addObject("cut", fg::parseShorthand("sphere(center=[4,0,0],radius=5)"));
        const int diff = doc.applyOperation("difference", {}, {id, cut});
        const SceneObject* c = doc.scene().find(diff);
        if (c) {
          const std::vector<double> sphere =
              values(fg::parseShorthand("sphere(center=[4,0,0],radius=5)"), points);
          std::vector<double> want;
          for (std::size_t i = 0; i < got.size(); ++i) want.push_back(std::max(got[i], -sphere[i]));
          check(maxDiff(values(fg::parseJson(fg::dumpJson(c->graph)), points), want) <= 1e-12,
                what + ": a difference on the fill is max(fill, -sphere)");
        } else {
          check(false, what + ": a difference on the fill");
        }
        doc.undoStack()->undo();
        doc.undoStack()->undo();
        doc.undoStack()->undo();
        check(sameScene(doc.scene(), before), what + ": undo restores the volume");
        doc.undoStack()->redo();
        const SceneObject* again = doc.scene().find(id);
        check(again && sameGraph(again->graph, graph), what + ": redo brings it back");
        doc.undoStack()->undo();
      }
    }
  }

  // 8. The skin, on the sphere (radius 10), with an sc lattice far and thin
  // enough to be positive at the probes. Inside within 2 mm of the surface is
  // solid, deeper and outside are not.
  {
    FillSpec spec;
    spec.kind = "sc";
    spec.wavelength = 100.0;
    spec.center = {50, 50, 50};  // a cell's struts cross at its centre, 50 mm away
    spec.radius = spec.nodeRadius = 0.01;
    spec.skin = 2.0;
    const int id = doc.applyFill(spec, ball.id);
    const SceneObject* r = doc.scene().find(id);
    if (r) {
      const std::vector<double> v =
          values(r->graph, {{0, 0, 9.0}, {0, 0, 8.5}, {0, 0, 7.5}, {0, 0, 10.5}});
      check(v[0] < 0 && v[1] < 0 && v[2] > 0 && v[3] > 0 && std::abs(v[0] + 1.0) < 1e-9,
            "the skin is the inside within 2 mm of the surface (-1 mm at depth 1 mm): " +
                number(v[0]) + ", " + number(v[1]) + ", " + number(v[2]) + ", " + number(v[3]));
      doc.undoStack()->undo();
    } else {
      check(false, "a skin fill of the sphere");
    }
  }

  // 4. Smooth booleans over non-metric operands lower to GLSL, as a scene of
  // their own (the document's would bake every result's meshes).
  Scene scene;
  for (const std::string& op : {std::string("smooth-union"), std::string("smooth-difference")}) {
    std::map<std::string, fg::ParamValue> params;
    params["k"].num = k;
    for (const std::vector<int>& ids : {std::vector<int>{ball.id, winding.id},
                                        std::vector<int>{octet.id, gyroid.id}}) {
      std::vector<const SceneObject*> objects;
      for (int o : ids) objects.push_back(doc.scene().find(o));
      SceneObject o;
      o.id = scene.nextId();
      o.name = QString::fromStdString(op);
      o.graph = operationGraph(op, params, objects);
      scene.insert(std::move(o), int(scene.objects().size()));
    }
  }
  try {
    const LoweredScene lowered = lowerScene(scene);
    check(lowered.objects.size() == 4,
          "smooth booleans over winding and gyroid operands lower to GLSL");
  } catch (const std::exception& e) {
    check(false, std::string("lowering: ") + e.what());
  }

  // 9. The catalogue is DualC's registries.
  {
    std::vector<std::string> booleans, tpms, struts;
    for (const OpEntry& e : operationCatalogue()) {
      if (e.family == OpFamily::Boolean) booleans.push_back(e.op);
      if (e.family == OpFamily::Tpms) tpms.push_back(e.op);
      if (e.family == OpFamily::Strut) struts.push_back(e.op);
    }
    const std::vector<std::string> expected = {"union", "intersection", "difference", "xor",
                                               "smooth-union", "smooth-intersection",
                                               "smooth-difference"};
    check(booleans == expected, "the catalogue's booleans are the seven DualC parses");
    bool allParse = true;
    for (const std::string& op : booleans) {
      dce::BoolOp b;
      allParse = allParse && dce::parseBoolOp(op, b) && isBooleanOp(op);
    }
    check(allParse, "every catalogue boolean parses as one of DualC's");
    std::vector<std::string> dualcTpms, dualcStruts;
    for (const dce::TpmsKind& k : dce::tpmsKinds()) dualcTpms.push_back(k.name);
    for (const dce::StrutKind& k : dce::strutKinds()) dualcStruts.push_back(k.name);
    check(!tpms.empty() && tpms == dualcTpms, "the catalogue's TPMS are tpmsKinds(), in order (" +
                                                  std::to_string(tpms.size()) + ")");
    check(!struts.empty() && struts == dualcStruts,
          "the catalogue's crystals are strutKinds(), in order (" + std::to_string(struts.size()) +
              ")");
    bool shapes = true;
    for (const OpEntry& e : operationCatalogue()) {
      if (e.family != OpFamily::Boolean) continue;
      shapes = shapes && e.smooth == isSmoothBooleanOp(e.op) &&
               e.params.size() == (e.smooth ? 1u : 0u) &&
               (!e.smooth || (e.params[0].key == "k" && e.params[0].value == 0.25));
    }
    check(shapes, "a smooth boolean has k at 0.25, a sharp one no parameter");
    check(operationTitle("smooth-intersection") == "Smooth Intersection" &&
              operationTitle("schwarz-p") == "Schwarz P" && operationTitle("xor") == "Xor",
          "menu titles from tokens");
    check(findOperation("no-such-op") == nullptr &&
              operationParams("no-such-op", dualc::BBox{}).empty(),
          "an unknown op has no entry and no parameters");
    dualc::BBox cube;
    cube.min = {-10, -10, -10};
    cube.max = {10, 10, 10};
    const FillSpec d = defaultFill("octet", cube);
    bool fillDefaults = true;
    for (const OpEntry& e : operationCatalogue()) {
      if (e.family == OpFamily::Boolean) continue;
      for (const OpParam& p : operationParams(e.op, cube)) {
        const double want = p.key == "wavelength" ? d.wavelength
                            : p.key == "thickness" ? d.thickness
                            : p.key == "radius" ? d.radius
                                                : d.nodeRadius;
        fillDefaults = fillDefaults && p.value == want;
      }
    }
    check(fillDefaults, "a fill's parameters start at defaultFill of the volume's box");
  }

  // 10. Every parameter is read by DualC's builder.
  {
    dualc::BBox cube;
    cube.min = {-10, -10, -10};
    cube.max = {10, 10, 10};
    int params = 0;
    for (const OpEntry& e : operationCatalogue()) {
      for (const OpParam& p : operationParams(e.op, cube)) {
        ++params;
        fg::GraphNode at, raised;
        if (e.family == OpFamily::Boolean) {
          const auto graph = [&](double k) {
            return fg::parseShorthand(e.op + "(sphere(radius=5),sphere(center=[6,0,0],radius=5)," +
                                      p.key + "=" + number(k) + ")");
          };
          at = graph(p.value);
          raised = graph(p.value * 8.0);
        } else {
          const auto spec = [&](double scale) {
            FillSpec s = defaultFill(e.op, cube);
            s.kind = e.op;
            if (p.key == "wavelength") s.wavelength *= scale;
            if (p.key == "thickness") s.thickness *= scale;
            if (p.key == "radius") s.radius *= scale;
            if (p.key == "nodeRadius") s.nodeRadius *= scale;
            return s;
          };
          at = fillSource(spec(1.0));
          raised = fillSource(spec(1.5));
        }
        check(maxDiff(values(at, points), values(raised, points)) > 1e-6,
              e.op + ": raising " + p.key + " changes DualC's field");
      }
    }
    check(params > 0, "the catalogue has " + std::to_string(params) + " parameters");
  }

  // 11. The command-reference page names every operation.
  if (argc > 1) {
    QFile page(argv[1]);
    const bool read = page.open(QIODevice::ReadOnly);
    const std::string text = read ? page.readAll().toStdString() : std::string();
    std::string missing;
    for (const OpEntry& e : operationCatalogue())
      if (text.find("`" + e.op + "`") == std::string::npos) missing += " " + e.op;
    check(read && missing.empty(), std::string("the command reference names every operation") +
                                       (missing.empty() ? "" : ", missing:" + missing));
  }

  if (failures) {
    std::fprintf(stderr, "operations_test: %d check(s) failed\n", failures);
    return 1;
  }
  std::printf("operations_test: all checks passed\n");
  return 0;
}
