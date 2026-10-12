#include "graph_file.h"

#include "graph_scene.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <exception>
#include <string>
#include <utility>

namespace fg = dce::fieldgraph;

fg::MeshHandle GraphDirMeshResolver::resolve(const std::string& path) {
  const QString p = QString::fromStdString(path);
  if (!dir_.isEmpty() && QDir::isRelativePath(p)) {
    const QString beside = QDir(dir_).filePath(p);
    if (QFileInfo::exists(beside)) return files_.resolve(beside.toStdString());
  }
  return files_.resolve(path);
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
