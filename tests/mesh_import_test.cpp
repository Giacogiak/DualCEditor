// The mesh readers (P04 T1, E-09), GL-free: one fixture per format (OBJ, ASCII
// and binary PLY, ASCII and binary STL) gives the vertex and face counts after
// welding; the weld itself on a soup; a malformed file of each format gives an
// error message naming the file, not a crash; a read mesh registered by id
// resolves through GraphDirMeshResolver and lowers as `mesh(id=...)`, with no
// path in the graph.

#include "graph_file.h"
#include "mesh_import.h"
#include "mesh_library.h"
#include "scene.h"

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QtEndian>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace fg = dce::fieldgraph;

namespace {

int failures = 0;

void check(bool ok, const QString& what) {
  std::fprintf(ok ? stdout : stderr, "mesh_import_test: %s %s\n", ok ? "ok  " : "FAIL",
               what.toUtf8().constData());
  if (!ok) ++failures;
}

// A read that fails with a message containing `part` and the file's name.
void checkError(const QByteArray& bytes, MeshFormat format, const char* part, const char* what) {
  const MeshRead r = readMeshBytes(bytes, format, "bad.mesh");
  const bool ok = !r.mesh && r.error.startsWith("bad.mesh: ") && r.error.contains(part);
  check(ok, QString("%1 -> \"%2\"").arg(what, r.error));
}

QByteArray binaryStl(quint32 declared, int stored) {
  QByteArray b(80, ' ');
  char count[4];
  qToLittleEndian<quint32>(declared, count);
  b.append(count, 4);
  b.append(QByteArray(stored * 50, '\0'));
  return b;
}

fg::GraphNode meshNode(const std::string& id) {
  fg::GraphNode n;
  n.op = "mesh";
  n.params["id"].kind = fg::ParamValue::Kind::String;
  n.params["id"].str = id;
  n.pointer = "/root";
  return n;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: mesh_import_test <tests/data/meshes>\n");
    return 2;
  }
  const QString dir = QString::fromLocal8Bit(argv[1]);

  // One fixture per format, counted after welding.
  struct Fixture {
    const char* file;
    std::uint32_t readVertices, vertices, triangles;
  };
  const Fixture fixtures[] = {
      {"cube.obj", 24, 8, 12},         // 4 vertices per face, quads, v//n, v/t/n, negative
      {"cube_binary.ply", 24, 8, 12},  // binary little-endian, quads
      {"tetra_ascii.ply", 4, 4, 4},    // ASCII, already welded
      {"cube_ascii.stl", 36, 8, 12},
      {"cube_binary.stl", 36, 8, 12},  // header begins "solid", read as binary by its size
  };
  MeshBuffers cube;
  for (const Fixture& f : fixtures) {
    const MeshRead r = readMeshFile(dir + "/" + f.file);
    const bool ok = r.mesh && r.readVertices == f.readVertices &&
                    r.mesh->vertexCount() == f.vertices && r.mesh->triangleCount() == f.triangles &&
                    r.collapsed == 0;
    check(ok, QString("%1: %2 vertices read, %3 vertices and %4 triangles after welding (got "
                      "%5, %6, %7) %8")
                  .arg(f.file)
                  .arg(f.readVertices)
                  .arg(f.vertices)
                  .arg(f.triangles)
                  .arg(r.readVertices)
                  .arg(r.mesh ? r.mesh->vertexCount() : 0)
                  .arg(r.mesh ? r.mesh->triangleCount() : 0)
                  .arg(r.error));
    if (r.mesh && std::strcmp(f.file, "cube_binary.stl") == 0) cube = *r.mesh;
  }
  // Every format reads the same cube: the same welded vertex set.
  {
    const MeshRead obj = readMeshFile(dir + "/cube.obj");
    const MeshRead ply = readMeshFile(dir + "/cube_binary.ply");
    const MeshRead stl = readMeshFile(dir + "/cube_ascii.stl");
    auto box = [](const MeshRead& r) {
      float lo = 1e9f, hi = -1e9f;
      if (r.mesh)
        for (const float v : r.mesh->vertices) lo = std::min(lo, v), hi = std::max(hi, v);
      return std::make_pair(lo, hi);
    };
    check(box(obj) == std::make_pair(-10.0f, 10.0f) && box(ply) == box(obj) && box(stl) == box(obj),
          "OBJ, PLY and STL read the same 20 mm cube");
  }

  // The weld on a soup: two triangles sharing an edge, a -0, and a sliver the
  // weld collapses.
  {
    const std::vector<float> v = {0, 0, 0,  1, 0, 0,  0, 1, 0,     // triangle 1
                                  1, 0, 0,  1, 1, 0,  -0.0f, 1, 0,  // triangle 2, -0 is 0
                                  5, 5, 5,  5, 5, 5,  6, 5, 5};    // collapses
    const std::vector<std::uint32_t> t = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    std::uint32_t collapsed = 99;
    const MeshBuffers w = weldVertices(v, t, &collapsed);
    check(w.vertexCount() == 4 && w.triangleCount() == 2 && collapsed == 1,
          QString("weld: 9 vertices give 4, 3 triangles give 2, 1 collapsed (got %1, %2, %3)")
              .arg(w.vertexCount())
              .arg(w.triangleCount())
              .arg(collapsed));
    check(w.indices == std::vector<std::uint32_t>{0, 1, 2, 1, 3, 2},
          "weld: vertices numbered in order of first use, the collapsed one's gone");
  }

  // Malformed files: an error message, not a crash.
  checkError("", MeshFormat::Obj, "no triangles", "OBJ: empty");
  checkError("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\n", MeshFormat::Obj,
             "line 4: vertex index 4 is out of range (3 vertices)", "OBJ: index out of range");
  checkError("v 0 0 zero\n", MeshFormat::Obj, "line 1: a vertex needs 3 numbers",
             "OBJ: a word for a number");
  checkError("v 0 0 0\nv 1 0 0\nf 1 2\n", MeshFormat::Obj, "line 3: a face needs at least 3",
             "OBJ: a two-vertex face");
  checkError("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 x\n", MeshFormat::Obj, "\"x\" is not a vertex index",
             "OBJ: a word for an index");
  checkError("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -1 -2 -4\n", MeshFormat::Obj,
             "line 4: relative index -4 reaches before the first vertex",
             "OBJ: a relative index before the first vertex");
  checkError("v 0 0 1e39\nv 1 0 0\nv 0 1 0\nf 1 2 3\n", MeshFormat::Obj, "not a finite float",
             "OBJ: a coordinate past float range");
  checkError("v 0 0 0\nv 0 0 0\nv 0 0 0\nf 1 2 3\n", MeshFormat::Obj, "all 1 collapse",
             "OBJ: every triangle collapses");
  checkError("\x89PNG\r\n\x1a\n binary junk", MeshFormat::Obj, "no triangles", "OBJ: binary junk");

  checkError("", MeshFormat::Ply, "no \"ply\" line", "PLY: empty");
  checkError("ply\nformat ascii 1.0\nelement vertex 3\n", MeshFormat::Ply, "no end_header",
             "PLY: no end_header");
  checkError("ply\nformat binary_little_endian 1.0\nelement vertex 4000000000\nproperty float x\n"
             "property float y\nproperty float z\nend_header\n",
             MeshFormat::Ply, "more elements", "PLY: a count of billions");
  checkError("ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\n"
             "property float z\nelement face 1\nproperty list uchar int vertex_indices\n"
             "end_header\n0 0 0\n1 0 0\n0 1 0\n3 0 1 7\n",
             MeshFormat::Ply, "face 1: vertex index 8 is out of range", "PLY: index out of range");
  checkError("ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\n"
             "property float z\nelement face 1\nproperty list uchar int vertex_indices\n"
             "end_header\n0 0 0\n1 0 0\n0 1 0\n3 0 1 -1\n",
             MeshFormat::Ply, "face 1: a negative vertex index", "PLY: a negative index");
  checkError("ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\n"
             "property float z\nend_header\n0 0 0\n1 0 0\n0 1 0\n",
             MeshFormat::Ply, "bad.mesh: ", "PLY: no face element");
  {
    // A binary PLY cut short in its faces.
    QFile f(dir + "/cube_binary.ply");
    check(f.open(QIODevice::ReadOnly), "the binary PLY fixture opens");
    const QByteArray whole = f.readAll();
    const MeshRead r = readMeshBytes(whole.left(whole.size() - 30), MeshFormat::Ply, "cut.ply");
    // happly does not check its reads: a cut file may read as zeros. What
    // matters is that it returns, with a mesh whose indices are in range or
    // an error.
    bool inRange = true;
    if (r.mesh)
      for (const std::uint32_t i : r.mesh->indices) inRange = inRange && i < r.mesh->vertexCount();
    check(inRange && (r.mesh || r.error.startsWith("cut.ply: ")),
          QString("PLY: a binary file cut short returns (%1)")
              .arg(r.mesh ? "a mesh in range" : r.error));
  }

  checkError("", MeshFormat::Stl, "too short", "STL: empty");
  checkError(binaryStl(1000, 2), MeshFormat::Stl, "the header says 1000 triangles",
             "STL: binary, truncated");
  checkError(binaryStl(0xffffffffu, 0), MeshFormat::Stl, "the header says 4294967295",
             "STL: binary, a count of billions");
  checkError(binaryStl(0, 0), MeshFormat::Stl, "no triangles", "STL: binary, no triangles");
  checkError("solid x\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n"
             "  endloop\n endfacet\nendsolid x\n",
             MeshFormat::Stl, "facet 1: a facet needs 3 vertices, it has 2", "STL: ASCII, 2 vertices");
  checkError("solid x\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 zero\n", MeshFormat::Stl,
             "facet 1: expected a number", "STL: ASCII, a word for a number");
  checkError("solid x\n facet normal 0 0 1\n  outer loop\n   vertex 0 0 0\n   vertex 1 0 0\n"
             "   vertex 0 1 0\n",
             MeshFormat::Stl, "facet 1: expected \"endloop\"", "STL: ASCII, cut short");
  checkError("solid nothing\nendsolid nothing\n", MeshFormat::Stl, "no triangles",
             "STL: ASCII, no facets");

  {
    const MeshRead r = readMeshFile(dir + "/no_such_file.stl");
    check(!r.mesh && r.error.startsWith("cannot read "), "a missing file: " + r.error);
    const MeshRead s = readMeshFile(dir + "/cube.3mf");
    check(!s.mesh && s.error.contains("not a mesh file"), "a .3mf: " + s.error);
    check(isMeshFileName("A.OBJ") && isMeshFileName("b.Ply") && isMeshFileName("c.stl") &&
              !isMeshFileName("d.json") && !isMeshFileName("e.3mf"),
          "isMeshFileName: .obj, .ply, .stl in any case");
  }

  // The library: an id from the content, the same buffers the same id.
  check(cube.vertexCount() == 8, "the binary STL cube was read");
  const std::string id = addLibraryMesh(cube);
  check(id.rfind("mesh-", 0) == 0 && id.size() == 21, QString("a library id: %1").arg(id.c_str()));
  check(addLibraryMesh(cube) == id, "the same buffers give the same id");
  const MeshRead tetra = readMeshFile(dir + "/tetra_ascii.ply");
  check(tetra.mesh && addLibraryMesh(*tetra.mesh) != id, "other buffers give another id");
  check(findLibraryMesh(id) && findLibraryMesh(id)->triangleCount() == 12 &&
            !findLibraryMesh("mesh-0000000000000000"),
        "findLibraryMesh finds an added id and nothing else");

  // Resolved by id through DualC's InMemoryMeshResolver: the field of the
  // closed cube is about -10 at its centre and about +10 10 mm outside a face.
  {
    GraphDirMeshResolver meshes{QString()};
    const fg::FieldGraph field = fg::FieldGraph::build(meshNode(id), meshes);
    const double inside = field.field().valueAt({0, 0, 0});
    const double outside = field.field().valueAt({20, 0, 0});
    check(std::abs(inside + 10.0) < 0.1 && std::abs(outside - 10.0) < 0.1,
          QString("mesh(id=...) is the cube's distance: %1 at the centre, %2 outside")
              .arg(inside)
              .arg(outside));
    bool threw = false;
    try {
      fg::FieldGraph::build(meshNode("mesh-0000000000000000"), meshes);
    } catch (const fg::GraphError& e) {
      threw = std::string(e.what()).find("no imported mesh") != std::string::npos;
    }
    check(threw, "an unknown id is a GraphError, not a crash");
  }

  // A scene object holding the id lowers to a shader: the graph has no path.
  {
    Scene scene;
    scene.add("cube", meshNode(id));
    const LoweredScene lowered = lowerScene(scene);
    const dualc::BBox& b = lowered.objects[0].localBounds;
    check(!lowered.scene.generatedSource.empty() && b.min.x <= -10.0 && b.max.x >= 10.0,
          "a scene of mesh(id=...) lowers, its box around the cube");
    check(scene.objects()[0].graph.params.count("path") == 0 &&
              lowered.scene.generatedSource.find(kMeshIdPath) == std::string::npos,
          "the object's graph keeps its id, and the shader names no path");
    fg::GraphNode winding = meshNode(id);
    winding.op = "winding";
    scene.add("soup", winding);
    check(lowerScene(scene).objects.size() == 2, "winding(id=...) lowers too");
  }

  std::printf("mesh_import_test: %s\n", failures ? "FAILED" : "passed");
  return failures ? 1 : 0;
}
