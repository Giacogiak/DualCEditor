#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <optional>
#include <vector>

// Mesh files read into the triangle buffers DualC's InMemoryMeshResolver takes
// (E-09: OBJ, PLY and STL; docs/design/07-mesh-import.md). OBJ and STL are read
// here; PLY goes through geometry-central's reader (happly). Every format ends
// the same way: polygons fanned into triangles, coincident vertices welded,
// triangles that the weld collapses dropped.

enum class MeshFormat { Obj, Ply, Stl };

// The format a file name's suffix names, case-insensitive; none for another.
std::optional<MeshFormat> meshFormatFor(const QString& path);
// The suffixes a mesh drop or File > Import accepts.
bool isMeshFileName(const QString& path);

// A welded triangle mesh: 3 floats (x, y, z) per vertex and 3 0-based indices
// per triangle, every index in range, no triangle repeating a vertex.
struct MeshBuffers {
  std::vector<float> vertices;
  std::vector<std::uint32_t> indices;
  std::uint32_t vertexCount() const { return std::uint32_t(vertices.size() / 3); }
  std::uint32_t triangleCount() const { return std::uint32_t(indices.size() / 3); }
};

// One read. On failure `mesh` is empty and `error` says what is wrong and where
// ("cube.obj: line 7: vertex index 9 is out of range (8 vertices)"). The counts
// say what the weld did.
struct MeshRead {
  std::optional<MeshBuffers> mesh;
  QString error;
  std::uint32_t readVertices = 0;   // vertices as the file lists them
  std::uint32_t readTriangles = 0;  // triangles after fanning polygons
  std::uint32_t collapsed = 0;      // triangles the weld collapsed, dropped
};

// Read mesh bytes in `format`. `name` only labels the error messages.
MeshRead readMeshBytes(const QByteArray& bytes, MeshFormat format, const QString& name);
// Read the file at `path`, its format taken from its suffix.
MeshRead readMeshFile(const QString& path);

// Weld a triangle soup: vertices with the same float coordinates (0 and -0
// alike) become one, in order of first use; unused vertices go; a triangle
// left with a repeated vertex is dropped and counted in `collapsed`. Indices
// must be in range.
MeshBuffers weldVertices(const std::vector<float>& vertices,
                         const std::vector<std::uint32_t>& indices,
                         std::uint32_t* collapsed = nullptr);
