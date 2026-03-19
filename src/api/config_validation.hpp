#pragma once

#include "loradriver/radio_config.hpp"

namespace loradriver {

/// Validates LoRa modulation parameters in config against V1 constraints.
///
/// Returns 0 if all parameters are within V1 range, or a unique diagnostic
/// detail code identifying the first offending parameter:
///   2010 = spreading_factor out of [7, 12]
///   2011 = bandwidth_khz not in {125, 250, 500}
///   2012 = coding_rate_denominator not in [5, 8]
///   2013 = preamble_length below minimum (< 6)
///
/// Rejection is deterministic and allocation-free. Hardware is not touched.
[[nodiscard]] int ValidateLoRaParams(const RadioConfig& config) noexcept;

}  // namespace loradriver
