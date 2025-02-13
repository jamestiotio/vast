// Copyright (c) 2022-present, Trail of Bits, Inc.

#pragma once

#include "vast/Util/Warnings.hpp"

VAST_RELAX_WARNINGS
#include <mlir/IR/Types.h>
#include <mlir/IR/Attributes.h>
VAST_RELAX_WARNINGS

//===----------------------------------------------------------------------===//
// Tablegen Interface Declarations
//===----------------------------------------------------------------------===//

/// Include the generated interface declarations.
#include "vast/Interfaces/TypedAttrInterface.h.inc"
