#include "mesh_export.h"

#include "example_common.h"
#include "graph_scene.h"
#include "scene_document.h"

#include "dualc/pipeline.h"

#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace fg = dce::fieldgraph;

namespace {

// Write the edits keyed by `n`'s pointers into its params; the count of those
// that name no param of matching shape goes to `unbaked`.
void bakeEdits(fg::GraphNode& n, const std::map<ParamKey, std::vector<float>>& edits,
               std::size_t& baked) {
  for (auto& [key, param] : n.params) {
    const auto e = edits.find({n.pointer, key});
    if (e == edits.end()) continue;
    const std::vector<float>& v = e->second;
    if (param.kind == fg::ParamValue::Kind::Number && v.size() == 1) {
      param.num = v[0];
      ++baked;
    } else if (param.kind == fg::ParamValue::Kind::Vector && v.size() == param.vec.size()) {
      std::copy(v.begin(), v.end(), param.vec.begin());
      ++baked;
    }
  }
  for (fg::GraphNode& c : n.in) bakeEdits(c, edits, baked);
}

}  // namespace

ExportFormat exportFormat(const QString& path) {
  const QString ext = QFileInfo(path).suffix().toLower();
  if (ext == "stl") return ExportFormat::Stl;
  if (ext == "3mf") return ExportFormat::ThreeMf;
  if (ext == "obj") return ExportFormat::Obj;
  return ExportFormat::Unknown;
}

bool exportTileable(ExportFormat format) {
  return format == ExportFormat::Stl || format == ExportFormat::ThreeMf;
}

fg::GraphNode exportGraph(const Scene& scene, const std::vector<int>& objects,
                          std::size_t* unbaked) {
  std::vector<const SceneObject*> operands;
  for (const SceneObject& o : scene.objects())
    if (std::find(objects.begin(), objects.end(), o.id) != objects.end()) operands.push_back(&o);
  if (operands.empty()) return {};
  std::map<ParamKey, std::vector<float>> edits;
  // One object: the posed operand of a union, taken out of it.
  fg::GraphNode graph = operationGraph("union", {}, operands, &edits);
  if (operands.size() == 1) graph = std::move(graph.in.front());
  std::size_t baked = 0;
  bakeEdits(graph, edits, baked);
  if (unbaked) *unbaked = edits.size() - baked;
  return graph;
}

std::shared_ptr<ExportField> buildExportField(const fg::GraphNode& graph, const QString& meshDir) {
  auto f = std::make_shared<ExportField>();
  f->meshes = std::make_unique<GraphDirMeshResolver>(meshDir);
  f->graph.emplace(fg::FieldGraph::build(graph, *f->meshes));
  const dualc::BBox b = f->field().bounds();
  if (!b.isValid())
    throw fg::GraphError("the field has no finite bounds (infinite source or repeat)",
                         graph.pointer);
  const dualc::Vector3 pad = b.extent() * 0.05;
  f->bounds = {b.min - pad, b.max + pad};
  return f;
}

int defaultTileDepth(int depth) { return std::max(2, depth - 2); }

int tilesPerAxis(int depth, int tileDepth) {
  const long long global = 1LL << depth, owned = (1LL << tileDepth) - 2;
  return owned >= global ? 1 : int((global + owned - 1) / owned);
}

int exportProbeDepth(int depth) { return std::max(2, std::min(depth - 1, kExportProbeDepth)); }

ExportProbe probeExport(const dualc::ImplicitField& field, const dualc::BBox& bounds, int depth,
                        const dualc::CancelToken* cancel) {
  ExportProbe p;
  p.depth = exportProbeDepth(depth);
  dualc::SamplerParams sp;
  sp.rootBounds = bounds;
  sp.maxDepth = p.depth;
  sp.minDepth = std::min(sp.minDepth, p.depth);
  dualc::Diagnostics diag;
  auto [mesh, geom, normals] =
      dualc::dualContourField(field, sp, dualc::ContourerParams{}, &diag, cancel);
  (void)normals;
  if (diag.emptyContour) return p;
  // One centroid per triangle the writer emits: a polygon of n corners is
  // n - 2 of them.
  std::vector<dualc::Vector3> pos(mesh->nVertices());
  for (auto v : mesh->vertices()) pos[v.getIndex()] = geom->inputVertexPositions[v];
  for (const std::vector<std::size_t>& f : mesh->getFaceVertexList()) {
    if (f.size() < 3) continue;
    dualc::Vector3 c{0, 0, 0};
    for (std::size_t v : f) {
      c.x += pos[v].x;
      c.y += pos[v].y;
      c.z += pos[v].z;
    }
    const double inv = 1.0 / double(f.size());
    c = {c.x * inv, c.y * inv, c.z * inv};
    for (std::size_t k = 2; k < f.size(); ++k) p.centroids.push_back(c);
  }
  return p;
}

ExportEstimate estimateExport(const ExportProbe& probe, const ExportSpec& spec) {
  ExportEstimate e;
  const double growth = std::pow(4.0, spec.depth - probe.depth);
  e.faces = double(probe.centroids.size()) * growth;
  e.peakFaces = e.faces;
  if (spec.tiled && spec.tileDepth < spec.depth) {
    // DualC's tile grid: `tiles` tiles of `owned` global cells along each axis
    // from the box's min corner.
    e.tiles = tilesPerAxis(spec.depth, spec.tileDepth);
    const double owned = double((1LL << spec.tileDepth) - 2) / double(1LL << spec.depth);
    const dualc::Vector3 ext = spec.bounds.extent();
    std::unordered_map<long long, long long> buckets;
    long long busiest = 0;
    const auto axis = [&](double c, double lo, double w) {
      if (w <= 0.0) return 0;
      const int i = int(std::floor((c - lo) / (w * owned)));
      return std::clamp(i, 0, e.tiles - 1);
    };
    for (const dualc::Vector3& c : probe.centroids) {
      const long long key =
          (static_cast<long long>(axis(c.x, spec.bounds.min.x, ext.x)) * e.tiles +
           axis(c.y, spec.bounds.min.y, ext.y)) * e.tiles +
          axis(c.z, spec.bounds.min.z, ext.z);
      busiest = std::max(busiest, ++buckets[key]);
    }
    e.peakFaces = double(busiest) * growth;
  }
  e.ramBytes = e.peakFaces * kExportBytesPerFace;
  return e;
}

ExportResult runExport(const dualc::ImplicitField& field, const ExportSpec& spec,
                       const dualc::CancelToken* cancel, dualc::ProgressSink* progress) {
  ExportResult r;
  const ExportFormat format = exportFormat(spec.path);
  if (format == ExportFormat::Unknown) {
    r.error = QString("unknown extension \"%1\": export writes .stl, .3mf or .obj")
                  .arg(QFileInfo(spec.path).suffix());
    return r;
  }
  dualc::SamplerParams sp;
  sp.rootBounds = spec.bounds;
  sp.maxDepth = spec.depth;
  const dualc::ContourerParams cp;
  const std::string path = spec.path.toStdString();
  if (spec.tiled && exportTileable(format)) {
    r.rc = format == ExportFormat::Stl
               ? dce::writeFieldTiledStl(field, path, sp, cp, spec.tileDepth, cancel, progress)
               : dce::writeFieldTiled3mf(field, path, sp, cp, spec.tileDepth, false, cancel,
                                         progress);
  } else {
    r.rc = dce::writeField(field, path, sp, cp, {}, &r.diag, cancel, progress);
    r.diagnosed = true;
  }
  r.status = r.rc == 0 ? ExportStatus::Written
             : r.rc == 3 ? ExportStatus::Cancelled
                         : ExportStatus::Failed;
  if (r.status == ExportStatus::Failed) r.error = QString::fromStdString(dce::lastError());
  return r;
}

QString shortCount(double n) {
  if (n >= 1e9) return QString("%1 G").arg(n / 1e9, 0, 'f', 1);
  if (n >= 1e6) return QString("%1 M").arg(n / 1e6, 0, 'f', 1);
  if (n >= 1e4) return QString("%1 k").arg(n / 1e3, 0, 'f', 0);
  return QString::number(std::llround(n));
}

QString shortBytes(double bytes) {
  const double mib = bytes / (1024.0 * 1024.0);
  if (mib >= 1024.0) return QString("%1 GiB").arg(mib / 1024.0, 0, 'f', 1);
  return QString("%1 MiB").arg(std::max(mib, 1.0), 0, 'f', 0);
}
