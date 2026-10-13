#include "operation_catalogue.h"

#include "example_common.h"
#include "scene_document.h"

#include <cctype>

namespace {

// The family parameter lists, at the defaults for a unit box. Keys are the
// names DualC's field-graph builder reads (k, wavelength, radius, nodeRadius)
// or, for the TPMS wall, onion's thickness, which FillSpec carries as such.
std::vector<OpParam> familyParams(OpFamily family, bool smooth) {
  switch (family) {
    case OpFamily::Boolean:
      if (!smooth) return {};
      return {{"k", "Blend k", "The blend width of the smooth booleans, in mm (DualC's k)",
               0.25}};
    case OpFamily::Tpms:
      return {{"wavelength", "Wavelength", "The cell side. Default: the volume's box diagonal / 10",
               1.0},
              {"thickness", "Wall thickness t",
               "Onion's t on the normalized TPMS: the wall is 2 t across. Default: wavelength / 5",
               0.2}};
    case OpFamily::Strut:
      return {{"wavelength", "Wavelength", "The cell side. Default: the volume's box diagonal / 10",
               1.0},
              {"radius", "Radius", "The strut radius at mid-span. Default: wavelength / 10", 0.1},
              {"nodeRadius", "Node radius",
               "The strut radius at the nodes; above the radius it tapers the struts. "
               "Default: the radius",
               0.1}};
  }
  return {};
}

std::vector<OpEntry> buildCatalogue() {
  std::vector<OpEntry> out;
  // BoolOp is an enum DualC does not list. boolOpName falls back to "union"
  // past its last value, so the walk stops where a name no longer parses back
  // to the value it came from.
  for (int i = 0;; ++i) {
    const auto op = static_cast<dce::BoolOp>(i);
    const std::string name = dce::boolOpName(op);
    dce::BoolOp back;
    if (!dce::parseBoolOp(name, back) || back != op) break;
    OpEntry e;
    e.op = name;
    e.smooth = dce::boolOpIsSmooth(op);
    e.params = familyParams(OpFamily::Boolean, e.smooth);
    out.push_back(e);
  }
  for (const dce::TpmsKind& k : dce::tpmsKinds())
    out.push_back({k.name, OpFamily::Tpms, k.blurb, false, familyParams(OpFamily::Tpms, false)});
  for (const dce::StrutKind& k : dce::strutKinds())
    out.push_back({k.name, OpFamily::Strut, k.blurb, false, familyParams(OpFamily::Strut, false)});
  return out;
}

}  // namespace

const std::vector<OpEntry>& operationCatalogue() {
  static const std::vector<OpEntry> catalogue = buildCatalogue();
  return catalogue;
}

const OpEntry* findOperation(const std::string& op) {
  for (const OpEntry& e : operationCatalogue())
    if (e.op == op) return &e;
  return nullptr;
}

std::vector<const OpEntry*> operationsOf(OpFamily family) {
  std::vector<const OpEntry*> out;
  for (const OpEntry& e : operationCatalogue())
    if (e.family == family) out.push_back(&e);
  return out;
}

std::string operationTitle(const std::string& op) {
  std::string out = op;
  bool start = true;
  for (char& c : out) {
    if (c == '-') c = ' ';
    if (start) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    start = c == ' ';
  }
  return out;
}

std::vector<OpParam> operationParams(const std::string& op, const dualc::BBox& box) {
  const OpEntry* e = findOperation(op);
  if (!e) return {};
  std::vector<OpParam> params = e->params;
  if (e->family == OpFamily::Boolean) return params;
  const FillSpec d = defaultFill(op, box);
  for (OpParam& p : params) {
    if (p.key == "wavelength") p.value = d.wavelength;
    else if (p.key == "thickness") p.value = d.thickness;
    else if (p.key == "radius") p.value = d.radius;
    else if (p.key == "nodeRadius") p.value = d.nodeRadius;
  }
  return params;
}
