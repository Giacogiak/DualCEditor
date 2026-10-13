#include "mesh_library.h"

#include <QCryptographicHash>

#include <map>
#include <mutex>
#include <utility>

namespace {

std::mutex& libraryMutex() {
  static std::mutex m;
  return m;
}

std::map<std::string, std::shared_ptr<const MeshBuffers>>& library() {
  static std::map<std::string, std::shared_ptr<const MeshBuffers>> meshes;
  return meshes;
}

}  // namespace

std::string addLibraryMesh(MeshBuffers mesh) {
  QCryptographicHash hash(QCryptographicHash::Sha256);
  const quint32 counts[2] = {mesh.vertexCount(), mesh.triangleCount()};
  hash.addData(QByteArrayView(reinterpret_cast<const char*>(counts), sizeof counts));
  hash.addData(QByteArrayView(reinterpret_cast<const char*>(mesh.vertices.data()),
                              qsizetype(mesh.vertices.size() * sizeof(float))));
  hash.addData(QByteArrayView(reinterpret_cast<const char*>(mesh.indices.data()),
                              qsizetype(mesh.indices.size() * sizeof(std::uint32_t))));
  const std::string id = "mesh-" + hash.result().toHex().left(16).toStdString();
  const std::lock_guard<std::mutex> lock(libraryMutex());
  library().emplace(id, std::make_shared<const MeshBuffers>(std::move(mesh)));
  return id;
}

std::shared_ptr<const MeshBuffers> findLibraryMesh(const std::string& id) {
  const std::lock_guard<std::mutex> lock(libraryMutex());
  const auto it = library().find(id);
  return it == library().end() ? nullptr : it->second;
}
