#include "scene_file.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QTemporaryDir>

#include <cstring>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "miniz.h"

namespace fg = dce::fieldgraph;

namespace {

constexpr const char* kSceneEntry = "scene.json";
constexpr const char* kFormat = "dcscene";
constexpr const char* kMeshPrefix = "meshes/";

// A mesh source reads a file named by its `path` param (DualC's field_graph:
// `mesh` and `winding`); an `id` names a host buffer and is left alone.
bool readsMeshFile(const fg::GraphNode& n) {
  if (n.op != "mesh" && n.op != "winding") return false;
  const auto it = n.params.find("path");
  return it != n.params.end() && it->second.kind == fg::ParamValue::Kind::String &&
         !it->second.str.empty();
}

// --- Graph <-> JSON --------------------------------------------------------
// Not DualC's canonical form (dumpJson): that one drops the node pointers,
// and a parameter edit is keyed by its node's pointer, which for a shorthand
// graph is a character offset no reparse gives back.

QJsonObject nodeToJson(const fg::GraphNode& n) {
  QJsonObject out;
  out["op"] = QString::fromStdString(n.op);
  QJsonObject params;
  for (const auto& [key, v] : n.params) {
    QJsonValue value;
    switch (v.kind) {
      case fg::ParamValue::Kind::Number:
        value = v.num;
        break;
      case fg::ParamValue::Kind::Vector: {
        QJsonArray a;
        for (double x : v.vec) a.append(x);
        value = a;
        break;
      }
      case fg::ParamValue::Kind::String:
        value = QString::fromStdString(v.str);
        break;
    }
    params[QString::fromStdString(key)] = value;
  }
  if (!params.isEmpty()) out["params"] = params;
  QJsonArray in;
  for (const fg::GraphNode& c : n.in) in.append(nodeToJson(c));
  if (!in.isEmpty()) out["in"] = in;
  out["pointer"] = QString::fromStdString(n.pointer);
  return out;
}

bool nodeFromJson(const QJsonValue& value, const QString& where, fg::GraphNode* n,
                  QString* error) {
  if (!value.isObject()) {
    *error = where + ": not a node";
    return false;
  }
  const QJsonObject o = value.toObject();
  if (!o["op"].isString()) {
    *error = where + ": no op";
    return false;
  }
  n->op = o["op"].toString().toStdString();
  n->pointer = o["pointer"].toString().toStdString();
  const QJsonObject params = o["params"].toObject();
  for (auto it = params.begin(); it != params.end(); ++it) {
    fg::ParamValue p;
    if (it->isDouble()) {
      p.kind = fg::ParamValue::Kind::Number;
      p.num = it->toDouble();
    } else if (it->isString()) {
      p.kind = fg::ParamValue::Kind::String;
      p.str = it->toString().toStdString();
    } else if (it->isArray()) {
      p.kind = fg::ParamValue::Kind::Vector;
      for (const QJsonValue x : it->toArray()) {
        if (!x.isDouble()) {
          *error = where + ": param " + it.key() + " holds a non-number";
          return false;
        }
        p.vec.push_back(x.toDouble());
      }
    } else {
      *error = where + ": param " + it.key() + " is not a number, a string or an array";
      return false;
    }
    n->params[it.key().toStdString()] = std::move(p);
  }
  const QJsonArray in = o["in"].toArray();
  for (int i = 0; i < in.size(); ++i) {
    fg::GraphNode c;
    if (!nodeFromJson(in[i], where + "/in/" + QString::number(i), &c, error)) return false;
    n->in.push_back(std::move(c));
  }
  return true;
}

QJsonArray floats(const float* v, std::size_t n) {
  QJsonArray a;
  for (std::size_t i = 0; i < n; ++i) a.append(double(v[i]));
  return a;
}

bool readFloats(const QJsonValue& value, std::size_t n, std::vector<float>* out) {
  if (!value.isArray()) return false;
  const QJsonArray a = value.toArray();
  if (n != 0 && std::size_t(a.size()) != n) return false;
  out->clear();
  for (const QJsonValue x : a) {
    if (!x.isDouble()) return false;
    out->push_back(float(x.toDouble()));
  }
  return true;
}

// The file a mesh path names for an object resolving against `meshDir`: beside
// it first, then as given, as GraphDirMeshResolver resolves it.
QString meshFile(const QString& path, const QString& meshDir) {
  if (!meshDir.isEmpty() && QDir::isRelativePath(path)) {
    const QString beside = QDir(meshDir).filePath(path);
    if (QFileInfo::exists(beside)) return beside;
  }
  return path;
}

// The meshes of a scene being written: one entry per distinct content.
struct MeshTable {
  std::map<QByteArray, QString> byHash;  // sha256 + suffix -> entry name
  std::map<QString, QByteArray> entries;

  // Point every mesh path of `n` at its entry, reading the file it names.
  bool embed(fg::GraphNode& n, const SceneObject& o, QString* error) {
    if (readsMeshFile(n)) {
      std::string& path = n.params["path"].str;
      const QString file = meshFile(QString::fromStdString(path), o.meshDir);
      QFile f(file);
      if (!f.open(QIODevice::ReadOnly)) {
        *error = QString("cannot read mesh %1 of object %2 (%3): %4")
                     .arg(QString::fromStdString(path), QString::number(o.id), o.name,
                          f.errorString());
        return false;
      }
      const QByteArray bytes = f.readAll();
      const QString suffix = QFileInfo(file).suffix().toLower();
      const QByteArray key =
          QCryptographicHash::hash(bytes, QCryptographicHash::Sha256) + suffix.toUtf8();
      auto it = byHash.find(key);
      if (it == byHash.end()) {
        QString name = kMeshPrefix + QString::number(entries.size());
        if (!suffix.isEmpty()) name += "." + suffix;
        entries[name] = bytes;
        it = byHash.emplace(key, name).first;
      }
      path = it->second.toStdString();
    }
    for (fg::GraphNode& c : n.in)
      if (!embed(c, o, error)) return false;
    return true;
  }
};

// A mesh entry name a reader extracts: meshes/<one plain file name>.
bool isMeshEntry(const QString& name) {
  if (!name.startsWith(kMeshPrefix)) return false;
  const QString file = name.mid(int(std::strlen(kMeshPrefix)));
  return !file.isEmpty() && file != "." && file != ".." && !file.contains('/') &&
         !file.contains('\\') && !file.contains(':');
}

// Where loaded scenes' meshes are extracted: one folder per process, removed
// at exit, with one subfolder per load.
QString newMeshFolder(QString* error) {
  static QTemporaryDir root(QDir::tempPath() + "/DualCEditor-XXXXXX");
  static int loads = 0;
  if (!root.isValid()) {
    *error = "cannot make a folder for the scene's meshes: " + root.errorString();
    return {};
  }
  const QString dir = root.filePath(QString("scene-%1").arg(++loads));
  if (!QDir().mkpath(dir + "/meshes")) {
    *error = "cannot make the folder " + dir;
    return {};
  }
  return dir;
}

}  // namespace

bool isSceneFileName(const QString& path) {
  return path.endsWith(".dcscene", Qt::CaseInsensitive);
}

QString writeZip(const QString& path, const std::map<QString, QByteArray>& entries) {
  mz_zip_archive zip;
  std::memset(&zip, 0, sizeof(zip));
  if (!mz_zip_writer_init_heap(&zip, 0, 0)) return "cannot start a zip archive";
  for (const auto& [name, bytes] : entries) {
    if (!mz_zip_writer_add_mem(&zip, name.toUtf8().constData(), bytes.constData(),
                               std::size_t(bytes.size()), MZ_DEFAULT_LEVEL)) {
      const QString e = mz_zip_get_error_string(mz_zip_get_last_error(&zip));
      mz_zip_writer_end(&zip);
      return QString("cannot add %1 to the archive: %2").arg(name, e);
    }
  }
  void* buffer = nullptr;
  std::size_t size = 0;
  if (!mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size)) {
    mz_zip_writer_end(&zip);
    return "cannot finish the zip archive";
  }
  const QByteArray data(static_cast<const char*>(buffer), qsizetype(size));
  mz_free(buffer);
  mz_zip_writer_end(&zip);

  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return QString("cannot write %1: %2").arg(path, file.errorString());
  if (file.write(data) != data.size() || !file.commit())
    return QString("cannot write %1: %2").arg(path, file.errorString());
  return {};
}

std::optional<std::map<QString, QByteArray>> readZip(const QByteArray& data, QString* error) {
  mz_zip_archive zip;
  std::memset(&zip, 0, sizeof(zip));
  if (!mz_zip_reader_init_mem(&zip, data.constData(), std::size_t(data.size()), 0)) {
    *error = "not a zip archive";
    return std::nullopt;
  }
  std::map<QString, QByteArray> out;
  const mz_uint n = mz_zip_reader_get_num_files(&zip);
  for (mz_uint i = 0; i < n; ++i) {
    mz_zip_archive_file_stat stat;
    if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
    if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;
    std::size_t size = 0;
    void* bytes = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
    if (!bytes) {
      *error = QString("cannot read %1 from the archive: %2")
                   .arg(QString::fromUtf8(stat.m_filename),
                        mz_zip_get_error_string(mz_zip_get_last_error(&zip)));
      mz_zip_reader_end(&zip);
      return std::nullopt;
    }
    out[QString::fromUtf8(stat.m_filename)] =
        QByteArray(static_cast<const char*>(bytes), qsizetype(size));
    mz_free(bytes);
  }
  mz_zip_reader_end(&zip);
  return out;
}

QString saveSceneFile(const Scene& scene, const QString& path) {
  const QDir sceneDir = QFileInfo(path).absoluteDir();
  MeshTable meshes;
  QJsonArray objects;
  for (const SceneObject& o : scene.objects()) {
    fg::GraphNode graph = o.graph;
    QString error;
    if (!meshes.embed(graph, o, &error)) return error;
    QJsonObject j;
    j["id"] = o.id;
    j["name"] = o.name;
    if (!o.file.isEmpty()) {
      // A resource name (":/...") is no path on disk; a file on another
      // drive has no relative name, and is left out.
      const QString rel = o.file.startsWith(':') ? o.file : sceneDir.relativeFilePath(o.file);
      if (!QDir::isAbsolutePath(rel) || rel.startsWith(':')) j["file"] = rel;
    }
    j["visible"] = o.visible;
    j["colour"] = floats(o.colour.data(), o.colour.size());
    QJsonArray m;
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c) m.append(o.transform.m[r][c]);
    j["transform"] = m;
    j["scale"] = o.scale;
    QJsonArray operands;
    for (int id : o.operands) operands.append(id);
    j["operands"] = operands;
    QJsonArray edits;
    for (const auto& [key, value] : o.edits)
      edits.append(QJsonObject{{"node", QString::fromStdString(key.first)},
                               {"param", QString::fromStdString(key.second)},
                               {"value", floats(value.data(), value.size())}});
    j["edits"] = edits;
    j["graph"] = nodeToJson(graph);
    objects.append(j);
  }
  QJsonArray meshNames;
  for (const auto& [name, bytes] : meshes.entries) meshNames.append(name);
  const QJsonObject root{{"format", kFormat},
                         {"version", kSceneFileVersion},
                         {"meshes", meshNames},
                         {"objects", objects}};
  std::map<QString, QByteArray> entries = std::move(meshes.entries);
  entries[kSceneEntry] = QJsonDocument(root).toJson(QJsonDocument::Indented);
  return writeZip(path, entries);
}

SceneLoad loadSceneFile(const QString& path) {
  SceneLoad out;
  const QString name = QFileInfo(path).fileName();
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    out.error = QString("cannot read %1: %2").arg(path, file.errorString());
    return out;
  }
  QString error;
  const std::optional<std::map<QString, QByteArray>> entries = readZip(file.readAll(), &error);
  if (!entries) {
    out.error = QString("%1 is not a scene file: %2").arg(name, error);
    return out;
  }
  const auto json = entries->find(kSceneEntry);
  if (json == entries->end()) {
    out.error = QString("%1 is not a scene file: it has no %2").arg(name, kSceneEntry);
    return out;
  }
  QJsonParseError parseError;
  const QJsonDocument doc = QJsonDocument::fromJson(json->second, &parseError);
  if (!doc.isObject()) {
    out.error = QString("%1: %2 is not a JSON object: %3")
                    .arg(name, kSceneEntry, parseError.errorString());
    return out;
  }
  const QJsonObject root = doc.object();
  if (root["format"].toString() != kFormat) {
    out.error = QString("%1 is not a scene file: %2 has no \"format\": \"%3\"")
                    .arg(name, kSceneEntry, kFormat);
    return out;
  }
  // The version first: a newer file may differ in anything else.
  const QJsonValue version = root["version"];
  if (!version.isDouble() || version.toDouble() != double(version.toInt())) {
    out.error = QString("%1 has no scene version; this DualCEditor reads version %2")
                    .arg(name)
                    .arg(kSceneFileVersion);
    return out;
  }
  if (version.toInt() != kSceneFileVersion) {
    out.error = QString("%1 is scene version %2, %3; this DualCEditor reads version %4")
                    .arg(name)
                    .arg(version.toInt())
                    .arg(version.toInt() > kSceneFileVersion
                             ? "written by a newer DualCEditor"
                             : "which no DualCEditor wrote")
                    .arg(kSceneFileVersion);
    return out;
  }

  QString meshDir;
  const QJsonArray meshNames = root["meshes"].toArray();
  for (const QJsonValue v : meshNames) {
    const QString entry = v.toString();
    const auto bytes = entries->find(entry);
    if (!isMeshEntry(entry) || bytes == entries->end()) {
      out.error = QString("%1: mesh \"%2\" is not in the archive").arg(name, entry);
      return out;
    }
    if (meshDir.isEmpty() && (meshDir = newMeshFolder(&out.error)).isEmpty()) return out;
    QFile mesh(QDir(meshDir).filePath(entry));
    if (!mesh.open(QIODevice::WriteOnly) || mesh.write(bytes->second) != bytes->second.size()) {
      out.error = QString("%1: cannot extract %2: %3").arg(name, entry, mesh.errorString());
      return out;
    }
  }

  const QDir sceneDir = QFileInfo(path).absoluteDir();
  Scene scene;
  std::set<int> ids;
  const QJsonArray objects = root["objects"].toArray();
  for (int i = 0; i < objects.size(); ++i) {
    const QJsonObject j = objects[i].toObject();
    const QString where = QString("%1: object %2").arg(name).arg(i);
    SceneObject o;
    o.id = j["id"].toInt();
    if (o.id <= 0 || !ids.insert(o.id).second) {
      out.error = where + ": no id, or one taken";
      return out;
    }
    o.name = j["name"].toString();
    o.visible = j["visible"].toBool(true);
    std::vector<float> v;
    if (!readFloats(j["colour"], 3, &v)) {
      out.error = where + ": colour is not three numbers";
      return out;
    }
    o.colour = {v[0], v[1], v[2]};
    const QJsonArray m = j["transform"].toArray();
    if (m.size() != 16) {
      out.error = where + ": transform is not 16 numbers";
      return out;
    }
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c) o.transform.m[r][c] = m[r * 4 + c].toDouble();
    // Optional: a file written before the gizmo has no scale.
    o.scale = j["scale"].toDouble(1.0);
    if (!(o.scale > 0.0)) {
      out.error = where + ": scale is not a positive number";
      return out;
    }
    for (const QJsonValue id : j["operands"].toArray()) o.operands.push_back(id.toInt());
    for (const QJsonValue e : j["edits"].toArray()) {
      const QJsonObject edit = e.toObject();
      if (!readFloats(edit["value"], 0, &v)) {
        out.error = where + ": an edit's value is not numbers";
        return out;
      }
      o.edits[{edit["node"].toString().toStdString(), edit["param"].toString().toStdString()}] =
          v;
    }
    if (!nodeFromJson(j["graph"], where + ": graph", &o.graph, &out.error)) return out;
    o.meshDir = meshDir;
    // The file it came from, when it is still there: watched for reloads.
    const QString rel = j["file"].toString();
    if (!rel.isEmpty()) {
      const QString abs =
          rel.startsWith(':') ? rel : QDir::cleanPath(sceneDir.absoluteFilePath(rel));
      if (QFileInfo::exists(abs)) o.file = abs;
    }
    scene.insert(std::move(o), int(scene.objects().size()));
  }
  out.scene = std::move(scene);
  return out;
}
