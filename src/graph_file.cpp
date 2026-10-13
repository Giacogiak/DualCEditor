#include "graph_file.h"

#include "graph_scene.h"
#include "mesh_library.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace fg = dce::fieldgraph;

fg::MeshHandle GraphDirMeshResolver::resolve(const std::string& path) {
  if (path.rfind(kMeshIdPath, 0) == 0) return resolveId(path.substr(std::strlen(kMeshIdPath)));
  const QString p = QString::fromStdString(path);
  if (!dir_.isEmpty() && QDir::isRelativePath(p)) {
    const QString beside = QDir(dir_).filePath(p);
    if (QFileInfo::exists(beside)) return files_.resolve(beside.toStdString());
  }
  return files_.resolve(path);
}

fg::MeshHandle GraphDirMeshResolver::resolveId(const std::string& id) {
  if (!registered_.count(id)) {
    const std::shared_ptr<const MeshBuffers> mesh = findLibraryMesh(id);
    if (!mesh) throw fg::GraphError("no imported mesh has the id '" + id + "'", "");
    ids_.registerMesh(id, mesh->vertices.data(), mesh->vertexCount(), mesh->indices.data(),
                      mesh->triangleCount());
    registered_.insert(id);
  }
  return ids_.resolveId(id);
}

fg::GraphNode meshIdsAsPaths(fg::GraphNode graph) {
  if (graph.op == "mesh" || graph.op == "winding") {
    const auto id = graph.params.find("id");
    if (id != graph.params.end() && id->second.kind == fg::ParamValue::Kind::String &&
        !graph.params.count("path")) {
      fg::ParamValue path = id->second;
      path.str = kMeshIdPath + path.str;
      graph.params.erase(id);
      graph.params["path"] = std::move(path);
    }
  }
  for (fg::GraphNode& c : graph.in) c = meshIdsAsPaths(std::move(c));
  return graph;
}

QString graphMeshDir(const QString& path) {
  // A Qt resource has no folder on disk to resolve meshes against.
  return path.startsWith(':') ? QString() : QFileInfo(path).absolutePath();
}

bool isGraphFileName(const QString& path) {
  return path.endsWith(".json", Qt::CaseInsensitive) ||
         path.endsWith(".fld", Qt::CaseInsensitive);
}

GraphLoad compileGraphFileText(const QByteArray& text, const QString& path) {
  GraphLoad out;
  try {
    GraphDirMeshResolver meshes(graphMeshDir(path));
    out.scene = compileGraphText(text.toStdString(),
                                 path.endsWith(".json", Qt::CaseInsensitive), meshes,
                                 &out.nodeOps);
  } catch (const fg::GraphError& e) {
    out.pointer = QString::fromStdString(e.pointer());
    out.error = QString("field-graph error%1%2: %3")
                    .arg(out.pointer.isEmpty() ? "" : " at ", out.pointer,
                         QString::fromUtf8(e.what()));
  } catch (const std::exception& e) {
    out.error = QString("field-graph error: %1").arg(QString::fromUtf8(e.what()));
  }
  return out;
}

GraphLoad loadGraphFile(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    GraphLoad out;
    out.error = QString("cannot read %1: %2").arg(path, file.errorString());
    return out;
  }
  return compileGraphFileText(file.readAll(), path);
}

GraphParse parseGraphFile(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    GraphParse out;
    out.error = QString("cannot read %1: %2").arg(path, file.errorString());
    return out;
  }
  return parseGraphFileText(file.readAll(), path);
}

GraphParse parseGraphFileText(const QByteArray& text, const QString& path) {
  GraphParse out;
  try {
    out.graph = parseGraphText(text.toStdString(),
                               path.endsWith(".json", Qt::CaseInsensitive));
  } catch (const fg::GraphError& e) {
    out.error = QString("field-graph error%1%2: %3")
                    .arg(e.pointer().empty() ? "" : " at ",
                         QString::fromStdString(e.pointer()), QString::fromUtf8(e.what()));
  } catch (const std::exception& e) {
    out.error = QString("field-graph error: %1").arg(QString::fromUtf8(e.what()));
  }
  return out;
}
