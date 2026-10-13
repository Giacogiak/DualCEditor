#pragma once

#include "mesh_import.h"

#include <memory>
#include <string>

// The meshes the scene's graphs name by id: a graph holds `mesh(id=...)` or
// `winding(id=...)`, never a file path (P04 T1, docs/design/07-mesh-import.md).
// One library per process, read and written from any thread (the compile
// thread resolves ids while the window imports). A mesh is never removed: an
// undone import can be redone, and the buffers are small next to the graphs'
// compiled fields.

// Add `mesh` and return its id: "mesh-" and the first 16 hex digits of the
// SHA-256 of its buffers. The same buffers give the same id, so a mesh
// imported twice is held once.
std::string addLibraryMesh(MeshBuffers mesh);

// The mesh registered under `id`, or null.
std::shared_ptr<const MeshBuffers> findLibraryMesh(const std::string& id);
