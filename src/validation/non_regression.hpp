#pragma once

// Internal header: re-exports the public non-regression API for use in the
// validation implementation. Helper factory functions (makeCase, makeBaseline)
// are defined locally in non_regression.cpp (anonymous namespace) to avoid
// duplicating their definitions across translation units.
#include <loradriver/non_regression.hpp>

#include <cstring>
