#include "mesh_solid.h"

#include "mesh_library.h"

#include "dualc/implicit.h"
#include "dualc/pipeline.h"
#include "geometrycentral/surface/surface_mesh.h"
#include "geometrycentral/surface/vertex_position_geometry.h"

#include <QStringList>

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace fg = dce::fieldgraph;

namespace {

// `mesh` as DualC's in-memory resolver holds it: a geometry-central mesh and
// its positions. The resolver owns both and must outlive their use.
fg::MeshHandle registered(fg::InMemoryMeshResolver& resolver, const MeshBuffers& mesh) {
  resolver.registerMesh("m", mesh.vertices.data(), mesh.vertexCount(), mesh.indices.data(),
                        mesh.triangleCount());
  return resolver.resolveId("m");
}

}  // namespace

QString MeshCheck::summary() const {
  if (diag.inputEmpty) return "no triangles";
  QStringList faults;
  if (diag.inputBoundaryEdges)
    faults << QString("%1 open edge%2").arg(diag.inputBoundaryEdges)
                  .arg(diag.inputBoundaryEdges == 1 ? "" : "s");
  if (diag.inputNonManifoldEdges)
    faults << QString("%1 non-manifold edge%2").arg(diag.inputNonManifoldEdges)
                  .arg(diag.inputNonManifoldEdges == 1 ? "" : "s");
  if (misorientedEdges)
    faults << QString("%1 edge%2 with flipped neighbours").arg(misorientedEdges)
                  .arg(misorientedEdges == 1 ? "" : "s");
  else if (watertight() && volume <= 0.0)
    faults << "normals inward";
  return faults.isEmpty() ? QString("watertight") : faults.join(", ");
}

MeshCheck checkMesh(const MeshBuffers& mesh) {
  MeshCheck c;
  if (mesh.triangleCount() == 0) {
    c.diag.inputEmpty = true;
    c.diag.inputWatertight = false;
    return c;
  }
  // DualC's own counts: the MeshSource dualContourMesh would build.
  fg::InMemoryMeshResolver resolver;
  const fg::MeshHandle h = registered(resolver, mesh);
  const dualc::MeshSource source(*h.mesh, *h.geom);
  c.diag.inputBoundaryEdges = source.boundaryEdgeCount();
  c.diag.inputNonManifoldEdges = source.nonManifoldEdgeCount();
  c.diag.inputWatertight = c.diag.inputBoundaryEdges == 0 && c.diag.inputNonManifoldEdges == 0;

  // Orientation: on a manifold edge the two triangles walk it in opposite
  // directions. Key each edge by its (low, high) ends and count the walks
  // from low to high and from high to low.
  std::map<std::pair<std::uint32_t, std::uint32_t>, std::pair<int, int>> edges;
  const std::vector<float>& v = mesh.vertices;
  for (std::size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
    const std::uint32_t tri[3] = {mesh.indices[t], mesh.indices[t + 1], mesh.indices[t + 2]};
    for (int k = 0; k < 3; ++k) {
      const std::uint32_t a = tri[k], b = tri[(k + 1) % 3];
      auto& walks = edges[{std::min(a, b), std::max(a, b)}];
      (a < b ? walks.first : walks.second) += 1;
    }
    // The divergence theorem: each triangle adds the signed volume of the
    // tetrahedron it makes with the origin.
    const auto p = [&](std::uint32_t i) {
      return dualc::Vector3{v[3 * i], v[3 * i + 1], v[3 * i + 2]};
    };
    const dualc::Vector3 p0 = p(tri[0]), p1 = p(tri[1]), p2 = p(tri[2]);
    c.volume += (p0.x * (p1.y * p2.z - p1.z * p2.y) - p0.y * (p1.x * p2.z - p1.z * p2.x) +
                 p0.z * (p1.x * p2.y - p1.y * p2.x)) /
                6.0;
  }
  for (const auto& [edge, walks] : edges)
    if (walks.first + walks.second == 2 && walks.first != 1) ++c.misorientedEdges;
  return c;
}

fg::GraphNode meshSourceNode(const std::string& op, const std::string& id) {
  fg::GraphNode n;
  n.op = op;
  n.params["id"].kind = fg::ParamValue::Kind::String;
  n.params["id"].str = id;
  n.pointer = "/root";
  return n;
}

MeshImport importMeshBuffers(MeshBuffers mesh) {
  MeshImport out;
  out.check = checkMesh(mesh);
  out.id = addLibraryMesh(std::move(mesh));
  out.graph = meshSourceNode(out.check.solid() ? "mesh" : "winding", out.id);
  return out;
}

MeshImport importMeshFile(const QString& path) {
  MeshRead read = readMeshFile(path);
  MeshImport out;
  if (read.mesh) {
    out = importMeshBuffers(*read.mesh);
  } else {
    out.error = read.error;
  }
  out.read = std::move(read);
  return out;
}

QString meshObjectKind(const fg::GraphNode& graph) {
  if ((graph.op != "mesh" && graph.op != "winding") || !graph.in.empty()) return {};
  const auto id = graph.params.find("id");
  if (id == graph.params.end() || id->second.kind != fg::ParamValue::Kind::String) return {};
  return QString::fromStdString(graph.op);
}

MeshBuffers makeWatertight(const MeshBuffers& mesh, int maxDepth,
                           const dualc::CancelToken* cancel, dualc::ProgressSink* progress,
                           dualc::Diagnostics* diag) {
  fg::InMemoryMeshResolver resolver;
  const fg::MeshHandle h = registered(resolver, mesh);
  const dualc::WindingNumberField field(*h.mesh, *h.geom);
  dualc::SamplerParams sampler;
  sampler.maxDepth = maxDepth;
  // The winding field's inside, not the input's raw triangles: a soup's
  // stray faces must not pull the surface (DualC's GWN path).
  sampler.signMethod = dualc::SignMethod::GENERALIZED_WINDING_NUMBER;
  dualc::Diagnostics local;
  if (!diag) diag = &local;
  auto [out, geom, normals] =
      dualc::dualContourField(field, sampler, dualc::ContourerParams{}, diag, cancel, progress);
  (void)normals;
  // No surface: DualC returns a placeholder triangle, not an empty mesh.
  if (diag->emptyContour) return {};

  // geometry-central's faces as a soup, fanned into triangles, then welded
  // as an import is.
  std::vector<float> vertices;
  vertices.reserve(out->nVertices() * 3);
  geometrycentral::surface::VertexData<std::size_t> index = out->getVertexIndices();
  std::vector<std::uint32_t> remap(out->nVertices());
  std::uint32_t next = 0;
  for (geometrycentral::surface::Vertex vx : out->vertices()) {
    const geometrycentral::Vector3 p = geom->vertexPositions[vx];
    vertices.insert(vertices.end(), {float(p.x), float(p.y), float(p.z)});
    remap[index[vx]] = next++;
  }
  std::vector<std::uint32_t> indices;
  for (geometrycentral::surface::Face f : out->faces()) {
    std::vector<std::uint32_t> corners;
    for (geometrycentral::surface::Vertex vx : f.adjacentVertices())
      corners.push_back(remap[index[vx]]);
    for (std::size_t k = 1; k + 1 < corners.size(); ++k)
      indices.insert(indices.end(), {corners[0], corners[k], corners[k + 1]});
  }
  return weldVertices(vertices, indices);
}
