#include "scene_document.h"

#include "example_common.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QUndoCommand>

#include <algorithm>
#include <utility>

namespace fg = dce::fieldgraph;

namespace {

// Ids for QUndoCommand::mergeWith.
constexpr int kParameterMergeId = 1;

class AddObject : public QUndoCommand {
 public:
  AddObject(SceneDocument* doc, SceneObject object, int index)
      : QUndoCommand(QString("Add %1").arg(object.name)),
        doc_(doc),
        object_(std::move(object)),
        index_(index) {}
  void redo() override { doc_->insertObject(object_, index_); }
  void undo() override { doc_->takeObject(object_.id); }

 private:
  SceneDocument* doc_;
  SceneObject object_;
  int index_;
};

class RemoveObject : public QUndoCommand {
 public:
  RemoveObject(SceneDocument* doc, SceneObject object, int index)
      : QUndoCommand(QString("Remove %1").arg(object.name)),
        doc_(doc),
        object_(std::move(object)),
        index_(index) {}
  void redo() override { doc_->takeObject(object_.id); }
  void undo() override { doc_->insertObject(object_, index_); }

 private:
  SceneDocument* doc_;
  SceneObject object_;
  int index_;
};

// One field of one object, from `before` to `after`.
template <typename T, T SceneObject::*Field>
class SetField : public QUndoCommand {
 public:
  SetField(SceneDocument* doc, int id, T before, T after, const QString& text)
      : QUndoCommand(text), doc_(doc), id_(id), before_(std::move(before)), after_(std::move(after)) {}
  void redo() override { set(after_); }
  void undo() override { set(before_); }

 private:
  void set(const T& value) {
    if (SceneObject* o = doc_->object(id_)) {
      o->*Field = value;
      doc_->touch(id_);
    }
  }
  SceneDocument* doc_;
  int id_;
  T before_, after_;
};

using RenameObject = SetField<QString, &SceneObject::name>;
using SetTransform = SetField<dualc::Mat4, &SceneObject::transform>;
using SetVisible = SetField<bool, &SceneObject::visible>;
using SetColour = SetField<SceneColour, &SceneObject::colour>;

// A pose: the transform and the scale in one step (the gizmo, the
// Transform dock).
class SetPose : public QUndoCommand {
 public:
  SetPose(SceneDocument* doc, int id, const SceneObject& before, const dualc::Mat4& transform,
          double scale, const QString& text)
      : QUndoCommand(text),
        doc_(doc),
        id_(id),
        before_{before.transform, before.scale},
        after_{transform, scale} {}
  void redo() override { set(after_); }
  void undo() override { set(before_); }

 private:
  struct Pose {
    dualc::Mat4 transform;
    double scale;
  };
  void set(const Pose& p) {
    if (SceneObject* o = doc_->object(id_)) {
      o->transform = p.transform;
      o->scale = p.scale;
      doc_->touch(id_);
    }
  }
  SceneDocument* doc_;
  int id_;
  Pose before_, after_;
};

class SetParameter : public QUndoCommand {
 public:
  SetParameter(SceneDocument* doc, int id, ParamKey key,
               std::optional<std::vector<float>> before, std::vector<float> current,
               std::vector<float> after)
      : QUndoCommand(QString("Edit %1").arg(QString::fromStdString(key.second))),
        doc_(doc),
        id_(id),
        key_(std::move(key)),
        before_(std::move(before)),
        current_(std::move(current)),
        after_(std::move(after)) {}
  int id() const override { return kParameterMergeId; }
  bool mergeWith(const QUndoCommand* other) override {
    const auto* o = static_cast<const SetParameter*>(other);
    if (o->id_ != id_ || o->key_ != key_) return false;
    after_ = o->after_;
    // Back to where it started: the step is gone, and so is an edit that
    // was not there before (the stack drops the step without undoing it).
    if (before_ ? after_ == *before_ : after_ == current_) {
      setObsolete(true);
      undo();
    }
    return true;
  }
  void redo() override { set(after_, after_); }
  void undo() override {
    if (before_)
      set(*before_, *before_);
    else
      set(std::nullopt, current_);
  }

 private:
  void set(const std::optional<std::vector<float>>& edit, const std::vector<float>& shown) {
    SceneObject* o = doc_->object(id_);
    if (!o) return;
    if (edit)
      o->edits[key_] = *edit;
    else
      o->edits.erase(key_);
    doc_->touchParam(id_, key_, shown);
  }
  SceneDocument* doc_;
  int id_;
  ParamKey key_;
  std::optional<std::vector<float>> before_;  // the edit before, none for none
  std::vector<float> current_;  // the uniform's value before
  std::vector<float> after_;
};

class ReplaceGraph : public QUndoCommand {
 public:
  ReplaceGraph(SceneDocument* doc, const SceneObject& o, fg::GraphNode graph, QString meshDir,
               const QString& verb)
      : QUndoCommand(QString("%1 %2").arg(verb.isEmpty() ? QString("Reload") : verb, o.name)),
        doc_(doc),
        id_(o.id),
        beforeGraph_(o.graph),
        beforeEdits_(o.edits),
        beforeMeshDir_(o.meshDir),
        after_(std::move(graph)),
        afterMeshDir_(meshDir.isEmpty() ? o.meshDir : std::move(meshDir)) {}
  void redo() override { set(after_, {}, afterMeshDir_); }
  void undo() override { set(beforeGraph_, beforeEdits_, beforeMeshDir_); }

 private:
  void set(const fg::GraphNode& graph, const std::map<ParamKey, std::vector<float>>& edits,
           const QString& meshDir) {
    SceneObject* o = doc_->object(id_);
    if (!o) return;
    o->graph = graph;
    o->edits = edits;
    o->meshDir = meshDir;
    doc_->touchGraph();
  }
  SceneDocument* doc_;
  int id_;
  fg::GraphNode beforeGraph_;
  std::map<ParamKey, std::vector<float>> beforeEdits_;
  QString beforeMeshDir_;
  fg::GraphNode after_;
  QString afterMeshDir_;
};

// The result is added after the last operand and the operands hidden; undo
// takes the result out and shows the operands that were shown.
class ApplyOperation : public QUndoCommand {
 public:
  ApplyOperation(SceneDocument* doc, SceneObject result, int index, std::vector<int> hidden,
                 const QString& text)
      : QUndoCommand(text),
        doc_(doc),
        result_(std::move(result)),
        index_(index),
        hidden_(std::move(hidden)) {}
  void redo() override {
    doc_->insertObject(result_, index_);
    setVisible(false);
  }
  void undo() override {
    doc_->takeObject(result_.id);
    setVisible(true);
  }

 private:
  void setVisible(bool visible) {
    for (int id : hidden_)
      if (SceneObject* o = doc_->object(id)) {
        o->visible = visible;
        doc_->touch(id);
      }
  }
  SceneDocument* doc_;
  SceneObject result_;
  int index_;
  std::vector<int> hidden_;  // the operands that were visible
};

void assignPointers(fg::GraphNode& n, const std::string& pointer,
                    std::map<std::string, std::string>& renamed) {
  renamed.emplace(n.pointer, pointer);
  n.pointer = pointer;
  for (std::size_t i = 0; i < n.in.size(); ++i)
    assignPointers(n.in[i], pointer + "/in/" + std::to_string(i), renamed);
}

void absoluteMeshPaths(fg::GraphNode& n, const QString& dir) {
  const auto it = n.params.find("path");
  if (it != n.params.end() && it->second.kind == fg::ParamValue::Kind::String) {
    const QString path = QString::fromStdString(it->second.str);
    if (QFileInfo(path).isRelative() && QFileInfo::exists(QDir(dir).filePath(path)))
      it->second.str = QDir(dir).absoluteFilePath(path).toStdString();
  }
  for (fg::GraphNode& c : n.in) absoluteMeshPaths(c, dir);
}

bool sameParam(const fg::ParamValue& a, const fg::ParamValue& b) {
  return a.kind == b.kind && a.num == b.num && a.vec == b.vec && a.str == b.str;
}

bool sameMatrix(const dualc::Mat4& a, const dualc::Mat4& b) {
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      if (a.m[i][j] != b.m[i][j]) return false;
  return true;
}

bool isIdentity(const dualc::Mat4& m) {
  return sameMatrix(m, dualc::Mat4::identity());
}

}  // namespace

SceneDocument::SceneDocument(QObject* parent) : QObject(parent) {
  // The end of every push, undo, redo and merge.
  connect(&stack_, &QUndoStack::indexChanged, this, &SceneDocument::flush);
}

void SceneDocument::reset(Scene scene) {
  stack_.clear();
  scene_ = std::move(scene);
  pending_ = {};
  pending_.structure = true;
  dirty_ = true;
  flush();
}

void SceneDocument::push(QUndoCommand* command) {
  stack_.push(command);
  flush();
}

int SceneDocument::addObject(QString name, fg::GraphNode graph, QString meshDir,
                             QString file) {
  SceneObject o;
  o.id = scene_.nextId();
  o.name = std::move(name);
  o.graph = std::move(graph);
  o.meshDir = std::move(meshDir);
  o.file = std::move(file);
  o.colour = sceneColourFor(o.id);
  const int id = o.id;
  push(new AddObject(this, std::move(o), int(scene_.objects().size())));
  return id;
}

void SceneDocument::removeObject(int id) {
  const SceneObject* o = scene_.find(id);
  if (!o) return;
  push(new RemoveObject(this, *o, scene_.indexOf(id)));
}

void SceneDocument::renameObject(int id, QString name) {
  const SceneObject* o = scene_.find(id);
  if (!o || o->name == name) return;
  push(new RenameObject(this, id, o->name, name,
                        QString("Rename %1 to %2").arg(o->name, name)));
}

void SceneDocument::setTransform(int id, const dualc::Mat4& worldFromLocal) {
  const SceneObject* o = scene_.find(id);
  if (!o || sameMatrix(o->transform, worldFromLocal)) return;
  push(new SetTransform(this, id, o->transform, worldFromLocal,
                        QString("Move %1").arg(o->name)));
}

void SceneDocument::setPose(int id, const dualc::Mat4& worldFromLocal, double scale,
                            const QString& verb) {
  const SceneObject* o = scene_.find(id);
  if (!o || !(scale > 0.0) || (sameMatrix(o->transform, worldFromLocal) && o->scale == scale))
    return;
  push(new SetPose(this, id, *o, worldFromLocal, scale,
                   QString("%1 %2").arg(verb.isEmpty() ? QString("Move") : verb, o->name)));
}

void SceneDocument::setVisible(int id, bool visible) {
  const SceneObject* o = scene_.find(id);
  if (!o || o->visible == visible) return;
  push(new SetVisible(this, id, o->visible, visible,
                      QString("%1 %2").arg(visible ? "Show" : "Hide", o->name)));
}

void SceneDocument::setColour(int id, const SceneColour& colour) {
  const SceneObject* o = scene_.find(id);
  if (!o || o->colour == colour) return;
  push(new SetColour(this, id, o->colour, colour, QString("Colour %1").arg(o->name)));
}

void SceneDocument::setParameter(int id, const ParamKey& key, std::vector<float> value,
                                 std::vector<float> current) {
  const SceneObject* o = scene_.find(id);
  if (!o) return;
  const auto e = o->edits.find(key);
  std::optional<std::vector<float>> before;
  if (e != o->edits.end()) before = e->second;
  if (before ? *before == value : current == value) return;
  push(new SetParameter(this, id, key, before, std::move(current), std::move(value)));
}

void SceneDocument::replaceGraph(int id, fg::GraphNode graph, QString meshDir,
                                 const QString& verb) {
  const SceneObject* o = scene_.find(id);
  if (!o) return;
  push(new ReplaceGraph(this, *o, std::move(graph), std::move(meshDir), verb));
}

int SceneDocument::applyOperation(const std::string& op,
                                  const std::map<std::string, fg::ParamValue>& params,
                                  const std::vector<int>& operands, QString name) {
  std::vector<const SceneObject*> objects;
  std::vector<int> hidden;
  int index = 0;
  for (int id : operands) {
    const SceneObject* o = scene_.find(id);
    if (!o) return 0;
    objects.push_back(o);
    if (o->visible) hidden.push_back(id);
    index = std::max(index, scene_.indexOf(id) + 1);
  }
  if (objects.empty()) return 0;
  SceneObject r;
  r.id = scene_.nextId();
  r.name = name.isEmpty() ? QString("%1 %2").arg(QString::fromStdString(op)).arg(r.id) : name;
  std::vector<std::size_t> normalized;
  r.graph = operationGraph(op, params, objects, &r.edits, &normalized);
  r.meshDir = objects.front()->meshDir;
  r.operands = operands;
  r.colour = sceneColourFor(r.id);
  const int id = r.id;
  QString text = r.name;
  if (!normalized.empty()) {
    QStringList names;
    for (std::size_t i : normalized) names << objects[i]->name;
    text += QString(" (normalize on %1)").arg(names.join(", "));
  }
  push(new ApplyOperation(this, std::move(r), index, std::move(hidden), text));
  return id;
}

void SceneDocument::insertObject(SceneObject object, int index) {
  scene_.insert(std::move(object), index);
  touchGraph();
}

std::optional<SceneObject> SceneDocument::takeObject(int id) {
  const SceneObject* o = scene_.find(id);
  if (!o) return std::nullopt;
  SceneObject out = *o;
  scene_.remove(id);
  touchGraph();
  return out;
}

void SceneDocument::touch(int id) {
  pending_.objects.insert(id);
  dirty_ = true;
}

void SceneDocument::touchGraph() {
  pending_.structure = true;
  dirty_ = true;
}

void SceneDocument::touchParam(int id, const ParamKey& key, std::vector<float> value) {
  pending_.params.push_back({id, key, std::move(value)});
  dirty_ = true;
}

void SceneDocument::beginMacro(const QString& text) {
  ++macroDepth_;
  stack_.beginMacro(text);
}

void SceneDocument::endMacro() {
  stack_.endMacro();
  --macroDepth_;
  flush();
}

void SceneDocument::flush() {
  if (!dirty_ || macroDepth_ > 0) return;
  Changes changes = std::move(pending_);
  pending_ = {};
  dirty_ = false;
  emit changed(changes);
}

bool isBooleanOp(const std::string& op) {
  dce::BoolOp bop;
  return dce::parseBoolOp(op, bop);
}

bool isSmoothBooleanOp(const std::string& op) {
  dce::BoolOp bop;
  return dce::parseBoolOp(op, bop) &&
         (bop == dce::BoolOp::SmoothUnion || bop == dce::BoolOp::SmoothIntersection ||
          bop == dce::BoolOp::SmoothDifference);
}

bool isMetricGraph(const fg::GraphNode& graph) {
  const std::string& op = graph.op;
  if (op == "normalize") return true;
  if (op == "winding" || op == "twist" || op == "bend" || op == "displace") return false;
  for (const dce::TpmsKind& k : dce::tpmsKinds())
    if (op == k.name) return false;
  for (const fg::GraphNode& in : graph.in)
    if (!isMetricGraph(in)) return false;
  return true;
}

fg::GraphNode operationGraph(const std::string& op,
                             const std::map<std::string, fg::ParamValue>& params,
                             const std::vector<const SceneObject*>& operands,
                             std::map<ParamKey, std::vector<float>>* edits,
                             std::vector<std::size_t>* normalized) {
  // A boolean takes two inputs: over more operands it folds left, in operand
  // order, op(op(a, b), c). Operand 0 sits under n - 1 nested nodes.
  const bool fold = isBooleanOp(op) && operands.size() > 2;
  const bool smooth = isSmoothBooleanOp(op);
  const std::size_t n = operands.size();
  const auto operandAt = [&](std::size_t i) {
    if (!fold) return "/root/in/" + std::to_string(i);
    std::string at = "/root";
    for (std::size_t j = 0; j + std::max<std::size_t>(i, 1) + 1 < n; ++j) at += "/in/0";
    return at + (i == 0 ? "/in/0" : "/in/1");
  };
  bool oneDir = true;
  for (const SceneObject* o : operands) oneDir = oneDir && o->meshDir == operands.front()->meshDir;
  std::vector<fg::GraphNode> inputs;
  for (std::size_t i = 0; i < n; ++i) {
    const SceneObject& o = *operands[i];
    fg::GraphNode graph = o.graph;
    if (!oneDir && !o.meshDir.isEmpty()) absoluteMeshPaths(graph, o.meshDir);
    const std::string at = operandAt(i);
    std::map<std::string, std::string> renamed;
    // Outermost first: normalize (E-10), transform, then scale, each only when
    // it is needed.
    const bool wrap = smooth && !isMetricGraph(o.graph);
    const bool moved = !isIdentity(o.transform), scaled = o.scale != 1.0;
    std::string inner = at;
    if (wrap) inner += "/in/0";
    const std::string transformAt = inner;
    if (moved) inner += "/in/0";
    const std::string scaleAt = inner;
    if (scaled) inner += "/in/0";
    assignPointers(graph, inner, renamed);
    if (scaled) {
      fg::GraphNode sn;
      sn.op = "scale";
      sn.params["s"].num = o.scale;
      sn.in.push_back(std::move(graph));
      sn.pointer = scaleAt;
      graph = std::move(sn);
    }
    if (moved) {
      fg::GraphNode t;
      t.op = "transform";
      fg::ParamValue matrix;
      matrix.kind = fg::ParamValue::Kind::Vector;
      for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) matrix.vec.push_back(o.transform.m[r][c]);
      t.params["matrix"] = std::move(matrix);
      t.in.push_back(std::move(graph));
      t.pointer = transformAt;
      graph = std::move(t);
    }
    if (wrap) {
      fg::GraphNode nn;
      nn.op = "normalize";
      nn.in.push_back(std::move(graph));
      nn.pointer = at;
      graph = std::move(nn);
      if (normalized) normalized->push_back(i);
    }
    inputs.push_back(std::move(graph));
    if (!edits) continue;
    for (const auto& [key, value] : o.edits) {
      const auto it = renamed.find(key.first);
      if (it != renamed.end()) (*edits)[{it->second, key.second}] = value;
    }
  }
  if (!fold) {
    fg::GraphNode root;
    root.op = op;
    root.params = params;
    root.in = std::move(inputs);
    root.pointer = "/root";
    return root;
  }
  fg::GraphNode acc = std::move(inputs[0]);
  for (std::size_t i = 1; i < n; ++i) {
    fg::GraphNode node;
    node.op = op;
    node.params = params;
    node.in.push_back(std::move(acc));
    node.in.push_back(std::move(inputs[i]));
    node.pointer = "/root";
    for (std::size_t j = i + 1; j < n; ++j) node.pointer += "/in/0";
    acc = std::move(node);
  }
  return acc;
}

bool sameGraph(const fg::GraphNode& a, const fg::GraphNode& b) {
  if (a.op != b.op || a.pointer != b.pointer || a.params.size() != b.params.size() ||
      a.in.size() != b.in.size())
    return false;
  for (auto i = a.params.begin(), j = b.params.begin(); i != a.params.end(); ++i, ++j)
    if (i->first != j->first || !sameParam(i->second, j->second)) return false;
  for (std::size_t i = 0; i < a.in.size(); ++i)
    if (!sameGraph(a.in[i], b.in[i])) return false;
  return true;
}

bool sameScene(const Scene& a, const Scene& b) {
  if (a.objects().size() != b.objects().size()) return false;
  for (std::size_t i = 0; i < a.objects().size(); ++i) {
    const SceneObject& x = a.objects()[i];
    const SceneObject& y = b.objects()[i];
    if (x.id != y.id || x.name != y.name || !sameGraph(x.graph, y.graph) ||
        x.meshDir != y.meshDir || x.file != y.file || x.edits != y.edits ||
        x.operands != y.operands || !sameMatrix(x.transform, y.transform) ||
        x.scale != y.scale ||
        x.visible != y.visible || x.colour != y.colour)
      return false;
  }
  return true;
}
