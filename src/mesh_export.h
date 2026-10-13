#pragma once

#include "field_graph.h"
#include "graph_file.h"
#include "scene.h"

#include "dualc/progress.h"
#include "dualc/types.h"

#include <QString>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Mesh export (P06 T1, docs/design/08-export.md): the selected object, or the
// visible scene, posed in world coordinates and contoured by DualC's writers
// (writeField, writeFieldTiledStl, writeFieldTiled3mf). Nothing here touches
// GL or a widget: the dialog and the worker thread are the window's.

// The output format, chosen by the file's extension (case-insensitive), as
// DualC's writeField chooses it.
enum class ExportFormat { Unknown, Stl, ThreeMf, Obj };
ExportFormat exportFormat(const QString& path);
// Whether DualC streams `format` in tiles: STL and 3MF, not OBJ.
bool exportTileable(ExportFormat format);

// The world graph of `objects` (scene ids, in scene order): one object posed
// as operationGraph poses an operand (transform over scale), several folded in
// a union. Each object's parameter edits are written into the graph's params,
// so the file is what the viewport shows; an edit of a uniform that is no
// param of its node (none of DualC's at the pinned commit) is counted in
// `unbaked`. Empty `in` and op when no id names an object.
dce::fieldgraph::GraphNode exportGraph(const Scene& scene, const std::vector<int>& objects,
                                       std::size_t* unbaked = nullptr);

// A built export field: the resolver owns the meshes the field refers to, so
// it lives as long as the field. Built once on the GUI thread, read from the
// worker (DualC's fields are safe to evaluate from any thread).
struct ExportField {
  std::unique_ptr<GraphDirMeshResolver> meshes;
  std::optional<dce::fieldgraph::FieldGraph> graph;
  dualc::BBox bounds;  // the field's own bounds padded by 5%, as traceBounds
  const dualc::ImplicitField& field() const { return graph->field(); }
};
// Throws dce::fieldgraph::GraphError when the graph does not build or the
// field is unbounded. `meshDir` is the folder relative mesh paths resolve in.
std::shared_ptr<ExportField> buildExportField(const dce::fieldgraph::GraphNode& graph,
                                              const QString& meshDir);

// What the dialog sets.
inline constexpr int kExportDefaultDepth = 7;
inline constexpr int kExportMinDepth = 3;
inline constexpr int kExportMaxDepth = 12;
struct ExportSpec {
  QString path;
  int depth = kExportDefaultDepth;  // the octree depth: 2^depth cells across the box
  dualc::BBox bounds;               // the sampled box, world mm
  bool tiled = false;               // DualC's streaming export, one tile in RAM (E-27)
  int tileDepth = 5;                // octree levels per tile, < depth
};
// The default tile depth for `depth`: depth - 2, at least 2, so a tile holds
// about a sixty-fourth of the box.
int defaultTileDepth(int depth);
// DualC's tile count per axis for `depth` and `tileDepth` (the one-cell ghost
// ring on each side: 2^tileDepth - 2 owned cells per tile).
int tilesPerAxis(int depth, int tileDepth);

// The face count and RAM an export will take, from one coarse contour (the
// probe) at min(depth - 1, kExportProbeDepth), grown by 4 per level: a surface
// in a fixed box has four times the faces when the cells halve. RAM is
// kExportBytesPerFace per face of what one contour holds: the whole mesh, or
// the busiest tile (the probe's face centroids bucketed into the tile grid, as
// DualC's --mem picker does). The constant is DualC's (example_common.cpp,
// kBytesPerSurfaceFace: the transient contour peak per output face, measured
// with a 1.6x margin).
inline constexpr int kExportProbeDepth = 5;
inline constexpr double kExportBytesPerFace = 3072.0;
struct ExportProbe {
  int depth = 0;
  std::vector<dualc::Vector3> centroids;  // one per probe face
};
ExportProbe probeExport(const dualc::ImplicitField& field, const dualc::BBox& bounds,
                        int depth, const dualc::CancelToken* cancel = nullptr);
struct ExportEstimate {
  double faces = 0.0;      // triangles in the file
  double peakFaces = 0.0;  // faces one contour holds: the whole mesh, or the busiest tile
  double ramBytes = 0.0;
  int tiles = 1;           // tiles per axis
};
ExportEstimate estimateExport(const ExportProbe& probe, const ExportSpec& spec);
// The probe depth the estimate of `depth` uses.
int exportProbeDepth(int depth);

// One export, run on any thread. DualC writes `path + ".part"` and renames it
// over `path` only on success: a cancelled or failed export leaves nothing at
// `path` (and leaves a file that was there untouched).
enum class ExportStatus { Written, Cancelled, Failed };
struct ExportResult {
  ExportStatus status = ExportStatus::Failed;
  int rc = 2;      // DualC's return code: 0 written, 1 unbounded, 2 writer, 3 cancelled
  QString error;   // DualC's lastError(), or the extension refused
  // DualC's diagnostics of the whole-mesh contour; a tiled export has none
  // (DualC's tiled writers report no Diagnostics), `diagnosed` is false then.
  dualc::Diagnostics diag;
  bool diagnosed = false;
};
ExportResult runExport(const dualc::ImplicitField& field, const ExportSpec& spec,
                       const dualc::CancelToken* cancel = nullptr,
                       dualc::ProgressSink* progress = nullptr);

// "1.2 M", "34 k", "560": a count for the dialog. Bytes as "1.5 GiB", "80 MiB".
QString shortCount(double n);
QString shortBytes(double bytes);
