#pragma once

#include "dualc/dualc.h"

#include <string>
#include <vector>

// The operations the Operations menu and its panels offer (P05 T3), generated
// from DualC's registries in example_common: every boolean BoolOp names, every
// TPMS of tpmsKinds() and every crystal of strutKinds(), in DualC's order. A
// new DualC op of one of these families appears here, in the menu and in its
// panel without an editor change.
//
// DualC's registries carry a token and a blurb, not the op's parameters, which
// its field-graph builder reads by name. So each family's parameters are
// written once here, per family and never per op: a smooth boolean's k, a
// TPMS fill's wavelength and wall t, a strut fill's wavelength, radius and node
// radius. The panels are built from these lists (operation_panel.h).

enum class OpFamily { Boolean, Tpms, Strut };

// One parameter of a panel: the key it is read back by (a graph param of
// DualC's builder, or a FillSpec field), a label, a tooltip saying where the
// default comes from, and the default. Every parameter is a length in mm.
struct OpParam {
  std::string key;
  std::string label;
  std::string tip;
  double value = 0.0;
};

struct OpEntry {
  std::string op;  // DualC's token: union, smooth-union, gyroid, octet, ...
  OpFamily family = OpFamily::Boolean;
  std::string blurb;
  bool smooth = false;          // a smooth boolean (DualC's boolOpIsSmooth)
  std::vector<OpParam> params;  // at the defaults for a unit box (see operationParams)
};

const std::vector<OpEntry>& operationCatalogue();
const OpEntry* findOperation(const std::string& op);
// The entries of one family, in catalogue order.
std::vector<const OpEntry*> operationsOf(OpFamily family);
// The menu text of a token: "smooth-union" gives "Smooth Union".
std::string operationTitle(const std::string& op);
// The parameters of `op` at the defaults for an operand whose world box is
// `box`: a fill's as defaultFill gives them, a smooth boolean's k at DualC's
// 0.25. Empty for an unknown op or a boolean with no parameter.
std::vector<OpParam> operationParams(const std::string& op, const dualc::BBox& box);
