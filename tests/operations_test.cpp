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

#include "graph_file.h"
#include "mesh_import.h"
#include "mesh_solid.h"
#include "scene.h"
#include "scene_document.h"

#include <QCoreApplication>
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

  if (failures) {
    std::fprintf(stderr, "operations_test: %d check(s) failed\n", failures);
    return 1;
  }
  std::printf("operations_test: all checks passed\n");
  return 0;
}
