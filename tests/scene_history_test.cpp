// The scene's undo history without GL: a sequence of commands (add, rename,
// move, hide, recolour, parameter edits, an operation, a reload, a remove) is
// undone to the empty scene and redone to the end. At every step the scene is
// the one recorded there, object for object, and lowers to the same shader
// source and the same uniform values.

#include "graph_file.h"
#include "graph_scene.h"
#include "scene.h"
#include "scene_document.h"

#include <QCoreApplication>
#include <QUndoStack>

#include <cstdio>
#include <string>
#include <vector>

namespace fg = dce::fieldgraph;

namespace {

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "scene_history_test: FAIL %s\n", what);
    ++failures;
  }
}

// What the viewport would get: the shader source and every uniform value.
std::string lowered(const Scene& scene) {
  if (scene.isEmpty()) return "empty";
  const LoweredScene l = lowerScene(scene);
  std::string out = l.scene.generatedSource;
  for (const fg::UniformBinding& b : l.scene.bindings) {
    out += "\n" + b.name + " =";
    for (float v : b.value) out += " " + std::to_string(v);
  }
  return out;
}

struct Step {
  Scene scene;
  std::string lowered;
};

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  SceneDocument doc;
  std::vector<SceneDocument::Changes> seen;
  QObject::connect(&doc, &SceneDocument::changed,
                   [&](const SceneDocument::Changes& c) { seen.push_back(c); });

  std::vector<Step> steps{{doc.scene(), lowered(doc.scene())}};
  const auto record = [&] { steps.push_back({doc.scene(), lowered(doc.scene())}); };

  const int a = doc.addObject("a", parseGraphText("sphere(radius=0.5)", false));
  record();
  const int b = doc.addObject("b", parseGraphText("box(min=[-0.4,-0.4,-0.4],max=[0.4,0.4,0.4])",
                                                  false));
  record();
  const int c = doc.addObject(
      "c", parseGraphText(
               "intersection(box(min=[-1,-1,-1],max=[1,1,1]),gyroid(wavelength=0.5))", false));
  record();
  check(a == 1 && b == 2 && c == 3, "ids count from 1");
  check(seen.size() == 3 && seen.back().structure, "an add is a structural change");

  doc.renameObject(b, "cube");
  record();
  check(doc.scene().find(b)->name == "cube", "rename");
  check(!seen.back().structure && seen.back().objects.count(b), "a rename is no rebuild");

  doc.setTransform(b, dualc::Mat4::translation({1.5, 0.0, 0.0}));
  record();
  check(!seen.back().structure, "a move is no rebuild");

  doc.setVisible(c, false);
  record();
  check(!seen.back().structure && seen.back().objects.count(c), "a hide is no rebuild");

  doc.setColour(a, {0.1f, 0.2f, 0.3f});
  record();

  // Two edits of one parameter merge into one undo step.
  const LoweredScene before = lowerScene(doc.scene());
  std::string radiusPointer;
  std::vector<float> radius;
  for (const fg::UniformBinding& bind : before.scene.bindings)
    if (bind.paramKey == "radius" && bind.name.rfind("u_o1_", 0) == 0) {
      radiusPointer = bind.nodePointer;
      radius = bind.value;
    }
  check(radius.size() == 1 && radius[0] == 0.5f, "the sphere's radius is a uniform");
  const int stepsBefore = doc.undoStack()->count();
  doc.setParameter(a, {radiusPointer, "radius"}, {0.6f}, radius);
  doc.setParameter(a, {radiusPointer, "radius"}, {0.7f}, {0.6f});
  record();
  check(doc.undoStack()->count() == stepsBefore + 1, "parameter edits merge");
  check(!seen.back().structure && seen.back().params.size() == 1 &&
            seen.back().params[0].value == std::vector<float>{0.7f},
        "a parameter edit is one uniform");
  check(steps.back().lowered.find("u_o1_n0_radius = 0.700000") != std::string::npos ||
            steps.back().lowered.find("radius = 0.700000") != std::string::npos,
        "the edit lowers into the uniform's value");

  // The union of a and the moved cube: a new object, operands hidden.
  const int u = doc.applyOperation("union", {}, {a, b});
  record();
  const SceneObject* uo = doc.scene().find(u);
  check(u == 4 && uo && uo->operands == std::vector<int>{a, b}, "the operation's object");
  check(uo && uo->graph.op == "union" && uo->graph.in.size() == 2 &&
            uo->graph.in[1].op == "transform" && uo->graph.in[0].pointer == "/root/in/0",
        "the operation's graph: the operands, the moved one in its transform");
  check(!doc.scene().find(a)->visible && !doc.scene().find(b)->visible,
        "the operands are hidden");
  check(doc.scene().objects().size() == 4, "the operands are kept");
  check(uo && uo->edits.size() == 1 && uo->edits.begin()->first.first == "/root/in/0" &&
            uo->edits.begin()->second == std::vector<float>{0.7f},
        "the operand's parameter edit follows it into the operation");

  doc.replaceGraph(c, parseGraphText(
                          "intersection(box(min=[-1,-1,-1],max=[1,1,1]),schwarz_p(wavelength=0.5))",
                          false));
  record();

  doc.removeObject(b);
  record();
  check(!doc.scene().find(b), "remove");

  // Undo to the start, then redo to the end: each step exactly as recorded.
  const int last = int(steps.size()) - 1;
  for (int i = last - 1; i >= 0; --i) {
    doc.undoStack()->undo();
    char what[64];
    std::snprintf(what, sizeof what, "undo to step %d: scene", i);
    check(sameScene(doc.scene(), steps[std::size_t(i)].scene), what);
    std::snprintf(what, sizeof what, "undo to step %d: lowered", i);
    check(lowered(doc.scene()) == steps[std::size_t(i)].lowered, what);
  }
  check(doc.scene().isEmpty(), "undone to the empty scene");
  for (int i = 1; i <= last; ++i) {
    doc.undoStack()->redo();
    char what[64];
    std::snprintf(what, sizeof what, "redo to step %d: scene", i);
    check(sameScene(doc.scene(), steps[std::size_t(i)].scene), what);
    std::snprintf(what, sizeof what, "redo to step %d: lowered", i);
    check(lowered(doc.scene()) == steps[std::size_t(i)].lowered, what);
  }

  // Undo of the parameter edits reports the value the uniform had.
  while (doc.undoStack()->index() > stepsBefore) doc.undoStack()->undo();
  check(doc.scene().find(a)->edits.empty(), "the undone edit is gone");
  check(!seen.back().params.empty() && seen.back().params.back().value == radius,
        "the undone edit puts the graph's value back on screen");

  // An edit back to its start leaves no step.
  doc.setParameter(a, {radiusPointer, "radius"}, {0.9f}, radius);
  doc.setParameter(a, {radiusPointer, "radius"}, radius, {0.9f});
  check(doc.undoStack()->count() == stepsBefore && doc.scene().find(a)->edits.empty(),
        "an edit undone by hand is no step");

  // Ids are not reused: undo the adds, add again.
  while (doc.undoStack()->canUndo()) doc.undoStack()->undo();
  check(doc.addObject("d", parseGraphText("sphere(radius=1)", false)) == 5,
        "an undone add's id is not reused");

  // A macro is one step and one change.
  const std::size_t changes = seen.size();
  doc.beginMacro("two");
  doc.addObject("e", parseGraphText("sphere(radius=1)", false));
  doc.addObject("f", parseGraphText("sphere(radius=1)", false));
  doc.endMacro();
  check(seen.size() == changes + 1 && doc.scene().objects().size() == 3, "a macro");
  doc.undoStack()->undo();
  check(seen.size() == changes + 2 && doc.scene().objects().size() == 1, "a macro's undo");

  // A reset is a new document: no history.
  doc.reset(Scene{});
  check(doc.undoStack()->count() == 0 && doc.scene().isEmpty(), "reset");

  if (failures == 0) std::printf("scene_history_test: all checks passed (%d steps)\n", last);
  return failures == 0 ? 0 : 1;
}
