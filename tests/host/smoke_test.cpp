#include <cstdlib>

#include "loradriver/lora_driver.hpp"

namespace {

int RunSmoke() {
  using loradriver::LoRaDriver;
  using loradriver::LoRaError;
  using loradriver::RadioConfig;

  LoRaDriver driver;

  RadioConfig v1_config;
  v1_config.chip = RadioConfig::Chip::kSx1276;
  v1_config.band = RadioConfig::Band::k868;
  v1_config.dio_routing = RadioConfig::DioRouting::kDio0Only;
  if (driver.initialize(v1_config) != LoRaError::kOk) {
    return EXIT_FAILURE;
  }
  if (!driver.isInitialized()) {
    return EXIT_FAILURE;
  }

  if (driver.shutdown() != LoRaError::kOk) {
    return EXIT_FAILURE;
  }

  RadioConfig deferred_profile;
  deferred_profile.chip = RadioConfig::Chip::kSx126xStub;
  deferred_profile.band = RadioConfig::Band::k868;
  deferred_profile.dio_routing = RadioConfig::DioRouting::kDio0Only;
  if (driver.initialize(deferred_profile) != LoRaError::kUnsupportedProfile) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunSmoke();
}
