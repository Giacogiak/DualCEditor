#pragma once

#include "scene.h"

#include <QByteArray>
#include <QString>

#include <map>
#include <optional>

// The scene file, `.dcscene` (E-08, docs/design/05-dcscene-format.md): a zip
// holding `scene.json` (every object, its graph, transform, flags, colour,
// parameter edits and operands) and `meshes/<n>.<ext>`, the bytes of every mesh
// file a graph reads. Nothing in it is an absolute path: an object's file is
// named relative to the scene file's folder, and a mesh path is the name of
// its entry in the zip. So a scene moved to another folder, or to another
// machine, still loads.

// The version this build writes, and the only one it reads.
inline constexpr int kSceneFileVersion = 1;

bool isSceneFileName(const QString& path);

// Write `scene` to `path`, replacing it whole or not at all (QSaveFile).
// Returns an empty string, or what went wrong ("cannot read mesh ...").
QString saveSceneFile(const Scene& scene, const QString& path);

// Read a scene file. On failure `scene` is empty and `error` says why: not a
// zip, no scene.json, a version this build does not read, a malformed object.
// The meshes are extracted to a folder that lives as long as the process
// (sceneMeshFolder), which becomes each object's meshDir. An object's file is
// kept when it exists relative to the scene's folder, and dropped otherwise:
// the graph in the scene file is what loads.
struct SceneLoad {
  std::optional<Scene> scene;
  QString error;
};
SceneLoad loadSceneFile(const QString& path);

// The zip layer, public for the tests: entries by name, stored deflated.
QString writeZip(const QString& path, const std::map<QString, QByteArray>& entries);
std::optional<std::map<QString, QByteArray>> readZip(const QByteArray& data, QString* error);
