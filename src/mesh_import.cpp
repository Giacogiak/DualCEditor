#include "mesh_import.h"

#include "geometrycentral/surface/simple_polygon_mesh.h"

#include <QByteArrayView>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>

#include <array>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

// What a reader throws; readMeshBytes prefixes the file name.
struct ReadError {
  QString message;
};

[[noreturn]] void fail(QString message) { throw ReadError{std::move(message)}; }

// A polygon soup as the file lists it, before fanning and welding.
struct Soup {
  std::vector<double> positions;  // 3 per vertex
  std::vector<std::vector<std::int64_t>> polygons;
  std::vector<int> polygonLines;  // the line each polygon is on, 0 when none
};

// Fan every polygon into triangles, check every coordinate and index, weld.
MeshRead finish(const Soup& soup) {
  const std::size_t n = soup.positions.size() / 3;
  if (n >= std::numeric_limits<std::uint32_t>::max())
    fail(QString("%1 vertices is more than the editor can hold").arg(n));
  std::vector<float> vertices(soup.positions.size());
  for (std::size_t i = 0; i < soup.positions.size(); ++i) {
    vertices[i] = float(soup.positions[i]);
    if (!std::isfinite(vertices[i]))
      fail(QString("vertex %1 has a coordinate that is not a finite float").arg(i / 3 + 1));
  }
  std::vector<std::uint32_t> indices;
  for (std::size_t p = 0; p < soup.polygons.size(); ++p) {
    const auto& poly = soup.polygons[p];
    const QString where = soup.polygonLines.empty() || soup.polygonLines[p] == 0
                              ? QString("face %1").arg(p + 1)
                              : QString("line %1").arg(soup.polygonLines[p]);
    if (poly.size() < 3)
      fail(QString("%1: a face needs at least 3 vertices, it has %2").arg(where).arg(poly.size()));
    for (const std::int64_t i : poly) {
      if (i < 0) fail(QString("%1: a negative vertex index").arg(where));
      if (std::size_t(i) >= n)
        fail(QString("%1: vertex index %2 is out of range (%3 vertices)")
                 .arg(where)
                 .arg(i + 1)
                 .arg(n));
    }
    for (std::size_t k = 1; k + 1 < poly.size(); ++k) {
      indices.push_back(std::uint32_t(poly[0]));
      indices.push_back(std::uint32_t(poly[k]));
      indices.push_back(std::uint32_t(poly[k + 1]));
    }
  }
  if (indices.size() / 3 >= std::numeric_limits<std::uint32_t>::max())
    fail(QString("%1 triangles is more than the editor can hold").arg(indices.size() / 3));
  MeshRead out;
  out.readVertices = std::uint32_t(n);
  out.readTriangles = std::uint32_t(indices.size() / 3);
  MeshBuffers welded = weldVertices(vertices, indices, &out.collapsed);
  if (welded.indices.empty())
    fail(out.readTriangles == 0 ? QString("no triangles")
                                : QString("no triangles left after welding (all %1 collapse)")
                                      .arg(out.readTriangles));
  out.mesh = std::move(welded);
  return out;
}

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; }

// The whitespace-separated tokens of [begin, end).
std::vector<QByteArrayView> tokens(const char* begin, const char* end) {
  std::vector<QByteArrayView> out;
  const char* p = begin;
  while (p < end) {
    while (p < end && isSpace(*p)) ++p;
    const char* start = p;
    while (p < end && !isSpace(*p)) ++p;
    if (p > start) out.emplace_back(start, p - start);
  }
  return out;
}

// A number in the C locale, whatever the user's locale writes ("0,5" in Belgium).
bool toNumber(QByteArrayView token, double* value) {
  bool ok = false;
  *value = token.toDouble(&ok);
  return ok;
}

// --- OBJ ------------------------------------------------------------------

// `v x y z [w]` and `f a b c ...` with a, b, c as `v`, `v/t`, `v//n` or
// `v/t/n`, 1-based or negative (relative to the vertices so far). Every other
// statement (vt, vn, o, g, s, usemtl, mtllib, l, p) is skipped.
Soup readObj(const QByteArray& bytes) {
  Soup soup;
  const char* p = bytes.constData();
  const char* end = p + bytes.size();
  int line = 0;
  while (p < end) {
    const char* eol = static_cast<const char*>(std::memchr(p, '\n', std::size_t(end - p)));
    if (!eol) eol = end;
    ++line;
    const char* hash = static_cast<const char*>(std::memchr(p, '#', std::size_t(eol - p)));
    const std::vector<QByteArrayView> t = tokens(p, hash ? hash : eol);
    p = eol + 1;
    if (t.empty()) continue;
    if (t[0] == "v") {
      double xyz[3];
      if (t.size() < 4 || !toNumber(t[1], &xyz[0]) || !toNumber(t[2], &xyz[1]) ||
          !toNumber(t[3], &xyz[2]))
        fail(QString("line %1: a vertex needs 3 numbers").arg(line));
      soup.positions.insert(soup.positions.end(), xyz, xyz + 3);
    } else if (t[0] == "f") {
      std::vector<std::int64_t> face;
      const std::int64_t count = std::int64_t(soup.positions.size() / 3);
      for (std::size_t k = 1; k < t.size(); ++k) {
        const qsizetype slash = t[k].indexOf('/');
        bool ok = false;
        const qlonglong i = (slash < 0 ? t[k] : t[k].first(slash)).toLongLong(&ok);
        if (!ok || i == 0)
          fail(QString("line %1: \"%2\" is not a vertex index")
                   .arg(line)
                   .arg(QString::fromUtf8(t[k].toByteArray())));
        if (i < 0 && count + i < 0)
          fail(QString("line %1: relative index %2 reaches before the first vertex")
                   .arg(line)
                   .arg(i));
        face.push_back(i > 0 ? i - 1 : count + i);
      }
      soup.polygons.push_back(std::move(face));
      soup.polygonLines.push_back(line);
    }
  }
  return soup;
}

// --- STL ------------------------------------------------------------------

constexpr qsizetype kStlHeader = 84;     // 80 bytes of header, a uint32 count
constexpr qsizetype kStlTriangle = 50;   // normal, 3 vertices, uint16 attribute

void addTriangle(Soup& soup, const float* xyz9) {
  const std::int64_t first = std::int64_t(soup.positions.size() / 3);
  soup.positions.insert(soup.positions.end(), xyz9, xyz9 + 9);
  soup.polygons.push_back({first, first + 1, first + 2});
}

// The binary layout. The vertex order gives the orientation (counter-clockwise
// seen from outside, as the format says); the stored normal is not read, since
// writers often leave it zero.
Soup readBinaryStl(const QByteArray& bytes) {
  if (bytes.size() < kStlHeader)
    fail(QString("%1 bytes is too short for a binary STL (84 at least)").arg(bytes.size()));
  const quint32 n = qFromLittleEndian<quint32>(bytes.constData() + 80);
  const qint64 need = kStlHeader + qint64(n) * kStlTriangle;
  if (bytes.size() < need)
    fail(QString("the header says %1 triangles (%2 bytes), the file has %3 bytes")
             .arg(n)
             .arg(need)
             .arg(bytes.size()));
  Soup soup;
  soup.positions.reserve(std::size_t(n) * 9);
  soup.polygons.reserve(n);
  const char* t = bytes.constData() + kStlHeader;
  for (quint32 i = 0; i < n; ++i, t += kStlTriangle) {
    float xyz[9];
    for (int k = 0; k < 9; ++k) xyz[k] = qFromLittleEndian<float>(t + 12 + 4 * k);
    addTriangle(soup, xyz);
  }
  return soup;
}

bool keyword(QByteArrayView token, const char* word) {
  return token.compare(QByteArrayView(word), Qt::CaseInsensitive) == 0;
}

// solid [name] { facet normal n n n outer loop vertex x y z (x3) endloop
// endfacet } endsolid [name], possibly several solids in one file.
Soup readAsciiStl(const QByteArray& bytes) {
  const std::vector<QByteArrayView> t = tokens(bytes.constData(), bytes.constData() + bytes.size());
  Soup soup;
  std::size_t i = 0;
  int facet = 0;
  auto expect = [&](const char* word) {
    if (i >= t.size() || !keyword(t[i], word))
      fail(QString("facet %1: expected \"%2\"").arg(facet).arg(word));
    ++i;
  };
  auto number = [&]() {
    double v = 0.0;
    if (i >= t.size() || !toNumber(t[i], &v))
      fail(QString("facet %1: expected a number").arg(facet));
    ++i;
    return v;
  };
  while (i < t.size()) {
    // Skip a solid's name, up to its first facet or its end.
    if (!keyword(t[i], "facet")) {
      ++i;
      continue;
    }
    ++facet;
    ++i;
    expect("normal");
    for (int k = 0; k < 3; ++k) number();
    expect("outer");
    expect("loop");
    float xyz[9];
    int vertices = 0;
    while (i < t.size() && keyword(t[i], "vertex")) {
      ++i;
      double v[3] = {number(), number(), number()};
      if (vertices < 3)
        for (int k = 0; k < 3; ++k) xyz[3 * vertices + k] = float(v[k]);
      ++vertices;
    }
    if (vertices != 3)
      fail(QString("facet %1: a facet needs 3 vertices, it has %2").arg(facet).arg(vertices));
    expect("endloop");
    expect("endfacet");
    addTriangle(soup, xyz);
  }
  return soup;
}

// ASCII when the file starts with "solid" and is not exactly the size a binary
// header gives: many binary writers also begin their header with "solid".
Soup readStl(const QByteArray& bytes) {
  const bool solid = bytes.size() >= 5 && QByteArrayView(bytes).first(5).compare(
                                              QByteArrayView("solid"), Qt::CaseInsensitive) == 0;
  bool binarySize = false;
  if (bytes.size() >= kStlHeader) {
    const quint32 n = qFromLittleEndian<quint32>(bytes.constData() + 80);
    binarySize = kStlHeader + qint64(n) * kStlTriangle == bytes.size();
  }
  return solid && !binarySize ? readAsciiStl(bytes) : readBinaryStl(bytes);
}

// --- PLY ------------------------------------------------------------------

// Check the header before happly sees it: the magic, an end, and element
// counts the file can hold (each element takes at least one byte in any
// encoding), so a count of billions is refused instead of allocated.
void checkPlyHeader(const QByteArray& bytes) {
  if (!bytes.startsWith("ply")) fail("not a PLY file (no \"ply\" line)");
  const qsizetype endHeader = bytes.indexOf("end_header");
  if (endHeader < 0) fail("the header has no end_header");
  const qsizetype body = bytes.size() - endHeader;
  qint64 total = 0;
  for (const QByteArray& line : bytes.left(endHeader).split('\n')) {
    const std::vector<QByteArrayView> t = tokens(line.constData(), line.constData() + line.size());
    if (t.size() < 3 || t[0] != "element") continue;
    bool ok = false;
    const qint64 count = t[2].toLongLong(&ok);
    if (!ok || count < 0)
      fail(QString("element %1: \"%2\" is not a count")
               .arg(QString::fromUtf8(t[1].toByteArray()), QString::fromUtf8(t[2].toByteArray())));
    total += count;
    if (total > body)
      fail(QString("the header declares more elements (%1 so far) than the file has bytes (%2)")
               .arg(total)
               .arg(body));
  }
}

// geometry-central's reader (happly): ASCII and binary, `vertex` x y z and
// `face` vertex_indices (or vertex_index).
Soup readPly(const QByteArray& bytes) {
  checkPlyHeader(bytes);
  geometrycentral::surface::SimplePolygonMesh simple;
  try {
    std::istringstream in(bytes.toStdString(), std::ios::binary);
    simple.readMeshFromFile(in, "ply");
  } catch (const std::exception& e) {
    fail(QString::fromUtf8(e.what()).trimmed());
  }
  Soup soup;
  soup.positions.reserve(simple.vertexCoordinates.size() * 3);
  for (const auto& v : simple.vertexCoordinates) {
    soup.positions.push_back(v.x);
    soup.positions.push_back(v.y);
    soup.positions.push_back(v.z);
  }
  soup.polygons.reserve(simple.polygons.size());
  // happly reads list entries as size_t: a negative index wraps around and
  // fails the range check in finish.
  for (const auto& poly : simple.polygons) {
    std::vector<std::int64_t> face;
    for (const std::size_t i : poly)
      face.push_back(i > std::size_t(std::numeric_limits<std::int64_t>::max()) ? -1
                                                                               : std::int64_t(i));
    soup.polygons.push_back(std::move(face));
  }
  return soup;
}

// The weld key: a vertex's float bits, with -0 read as 0.
struct Key {
  std::array<std::uint32_t, 3> bits;
  bool operator==(const Key& o) const { return bits == o.bits; }
};
struct KeyHash {
  std::size_t operator()(const Key& k) const {
    std::uint64_t h = 1469598103934665603ull;
    for (const std::uint32_t b : k.bits) h = (h ^ b) * 1099511628211ull;
    return std::size_t(h);
  }
};

}  // namespace

std::optional<MeshFormat> meshFormatFor(const QString& path) {
  const QString suffix = QFileInfo(path).suffix().toLower();
  if (suffix == "obj") return MeshFormat::Obj;
  if (suffix == "ply") return MeshFormat::Ply;
  if (suffix == "stl") return MeshFormat::Stl;
  return std::nullopt;
}

bool isMeshFileName(const QString& path) { return meshFormatFor(path).has_value(); }

MeshBuffers weldVertices(const std::vector<float>& vertices,
                         const std::vector<std::uint32_t>& indices, std::uint32_t* collapsed) {
  MeshBuffers out;
  std::unordered_map<Key, std::uint32_t, KeyHash> seen;
  std::vector<std::uint32_t> remap(vertices.size() / 3, std::numeric_limits<std::uint32_t>::max());
  auto welded = [&](std::uint32_t i) {
    if (remap[i] != std::numeric_limits<std::uint32_t>::max()) return remap[i];
    Key key;
    for (int k = 0; k < 3; ++k) {
      const float c = vertices[3 * std::size_t(i) + k] == 0.0f ? 0.0f : vertices[3 * std::size_t(i) + k];
      std::memcpy(&key.bits[k], &c, sizeof c);
    }
    const auto [it, added] = seen.emplace(key, out.vertexCount());
    if (added)
      out.vertices.insert(out.vertices.end(), vertices.begin() + 3 * std::ptrdiff_t(i),
                          vertices.begin() + 3 * std::ptrdiff_t(i) + 3);
    return remap[i] = it->second;
  };
  std::uint32_t dropped = 0;
  for (std::size_t t = 0; t + 2 < indices.size(); t += 3) {
    const std::uint32_t a = welded(indices[t]), b = welded(indices[t + 1]),
                        c = welded(indices[t + 2]);
    if (a == b || b == c || a == c) {
      ++dropped;
      continue;
    }
    out.indices.insert(out.indices.end(), {a, b, c});
  }
  // A vertex only a dropped triangle used is still in out.vertices: rebuild
  // without it when there is one.
  if (dropped > 0) {
    std::vector<std::uint32_t> keep(out.vertexCount(), std::numeric_limits<std::uint32_t>::max());
    MeshBuffers compact;
    for (std::uint32_t& i : out.indices) {
      if (keep[i] == std::numeric_limits<std::uint32_t>::max()) {
        keep[i] = compact.vertexCount();
        compact.vertices.insert(compact.vertices.end(), out.vertices.begin() + 3 * std::ptrdiff_t(i),
                                out.vertices.begin() + 3 * std::ptrdiff_t(i) + 3);
      }
      i = keep[i];
    }
    compact.indices = std::move(out.indices);
    out = std::move(compact);
  }
  if (collapsed) *collapsed = dropped;
  return out;
}

MeshRead readMeshBytes(const QByteArray& bytes, MeshFormat format, const QString& name) {
  try {
    switch (format) {
      case MeshFormat::Obj: return finish(readObj(bytes));
      case MeshFormat::Ply: return finish(readPly(bytes));
      case MeshFormat::Stl: return finish(readStl(bytes));
    }
    fail("unknown mesh format");
  } catch (const ReadError& e) {
    MeshRead out;
    out.error = QString("%1: %2").arg(name, e.message);
    return out;
  } catch (const std::exception& e) {
    // An allocation that fails on a huge but consistent file.
    MeshRead out;
    out.error = QString("%1: %2").arg(name, QString::fromUtf8(e.what()));
    return out;
  }
}

MeshRead readMeshFile(const QString& path) {
  const QString name = QFileInfo(path).fileName();
  const std::optional<MeshFormat> format = meshFormatFor(path);
  if (!format) {
    MeshRead out;
    out.error = QString("%1: not a mesh file (.obj, .ply or .stl)").arg(name);
    return out;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    MeshRead out;
    out.error = QString("cannot read %1: %2").arg(path, file.errorString());
    return out;
  }
  return readMeshBytes(file.readAll(), *format, name);
}
