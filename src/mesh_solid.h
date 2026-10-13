#pragma once

#include "field_graph.h"
#include "mesh_import.h"

#include "dualc/progress.h"
#include "dualc/types.h"

#include <QString>
#include <cstddef>
#include <optional>
#include <string>

// What a read mesh becomes in the scene (P04 T2, docs/design/07-mesh-import.md
// § Import as a solid). A watertight, consistently oriented mesh with its
// normals out is a `mesh` node: DualC's distance to it is signed by the mesh
// itself. Any other mesh is a `winding` node: its generalized winding number
// gives an inside to open shells and soups. "Make watertight" turns a winding
// object into a mesh one by contouring that field.

// The facts the choice rests on. `diag` holds DualC's input-mesh fields
// (inputEmpty, inputBoundaryEdges, inputNonManifoldEdges, inputWatertight), read
// from DualC's MeshSource, which counts them as dualContourMesh does. The
// orientation is ours: DualC does not report it.
struct MeshCheck {
  dualc::Diagnostics diag;
  // Edges two triangles walk in the same direction: neighbours that disagree
  // on which side is out. Counted on manifold edges only.
  std::size_t misorientedEdges = 0;
  // The signed volume the triangles enclose, in the mesh's units cubed;
  // negative when every normal points in.
  double volume = 0.0;

  bool watertight() const { return !diag.inputEmpty && diag.inputWatertight; }
  bool oriented() const { return misorientedEdges == 0 && volume > 0.0; }
  // A `mesh` node, not a `winding` one.
  bool solid() const { return watertight() && oriented(); }
  // One line for the status bar and the log: "watertight", or what is wrong
  // ("12 open edges, 3 non-manifold edges, normals inward").
  QString summary() const;
};

MeshCheck checkMesh(const MeshBuffers& mesh);

// A read mesh made a scene object's graph: the mesh added to the library and
// named by id, as `mesh` when check.solid(), else `winding`. On a read error
// `graph` is empty and `error` is the reader's message.
struct MeshImport {
  std::optional<dce::fieldgraph::GraphNode> graph;
  MeshRead read;
  MeshCheck check;
  std::string id;
  QString error;
};

MeshImport importMeshFile(const QString& path);
// As importMeshFile, for buffers already read.
MeshImport importMeshBuffers(MeshBuffers mesh);

// The node `mesh` or `winding` over the library mesh `id`.
dce::fieldgraph::GraphNode meshSourceNode(const std::string& op, const std::string& id);

// "mesh" or "winding" when `graph` is one such node over a library mesh (an
// imported object), empty otherwise. The Objects dock shows it.
QString meshObjectKind(const dce::fieldgraph::GraphNode& graph);

// "Make watertight": contour the generalized winding number field of `mesh`
// with DualC (dualContourField, octree depth `maxDepth`), then clean the
// result as an import is cleaned (weldVertices: shared corners merged, collapsed
// triangles dropped). `cancel` and `progress` go to DualC: the sample and the
// contour stages report, and a requested token throws dualc::Cancelled from
// either. Runs on any thread; it touches no Qt object. `diag`, when given,
// receives DualC's contour diagnostics.
inline constexpr int kWatertightDepth = 7;
MeshBuffers makeWatertight(const MeshBuffers& mesh, int maxDepth = kWatertightDepth,
                           const dualc::CancelToken* cancel = nullptr,
                           dualc::ProgressSink* progress = nullptr,
                           dualc::Diagnostics* diag = nullptr);
