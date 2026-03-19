#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "loradriver/incident_snapshot.hpp"
#include "loradriver/lora_driver.hpp"

namespace {

using loradriver::IncidentSnapshot;
using loradriver::LoRaDriver;
using loradriver::LoRaError;
using loradriver::RadioConfig;
using loradriver::RadioEvent;

// Diagnostic codes for LoRa parameter validation failures (mirrors config_validation.cpp)
constexpr int kDiagInvalidSpreadingFactor = 2010;
constexpr int kDiagInvalidBandwidth = 2011;
constexpr int kDiagInvalidCodingRate = 2012;
constexpr int kDiagInvalidPreambleLength = 2013;

RadioConfig MakeV1LoRaConfig() {
  RadioConfig config;
  config.chip = RadioConfig::Chip::kSx1276;
  config.band = RadioConfig::Band::k868;
  config.dio_routing = RadioConfig::DioRouting::kDio0Only;
  config.spi_frequency_hz = 8000000;
  config.spreading_factor = 9;
  config.bandwidth_khz = 125;
  config.coding_rate_denominator = 5;
  config.sync_word = 0x12;
  config.tx_power_dbm = 14;
  config.crc_enabled = true;
  config.preamble_length = 8;
  return config;
}

bool Contains(const std::string& value, const std::string& token) {
  return value.find(token) != std::string::npos;
}

// --- Task 1: RadioConfig LoRa fields and V1 defaults ---

bool TestRadioConfigHasV1LoRaDefaults() {
  RadioConfig config;
  if (config.spreading_factor != 9) return false;
  if (config.bandwidth_khz != 125) return false;
  if (config.coding_rate_denominator != 5) return false;
  if (config.sync_word != 0x12) return false;
  if (config.tx_power_dbm != 14) return false;
  if (!config.crc_enabled) return false;
  if (config.preamble_length != 8) return false;
  return true;
}

bool TestDefaultRadioConfigBeginSucceeds() {
  // Default-constructed RadioConfig must be V1-valid (chip=kSx1276, band=k868,
  // dio=kDio0Only, spi=8MHz, SF9, BW125, CR4/5, preamble=8)
  LoRaDriver driver;
  return driver.begin(RadioConfig{}) == LoRaError::kOk;
}

bool TestRadioConfigTrivialCopyPreserved() {
  RadioConfig a = MakeV1LoRaConfig();
  a.spreading_factor = 11;
  a.bandwidth_khz = 250;
  const RadioConfig b = a;  // trivial copy
  if (b.spreading_factor != 11) return false;
  if (b.bandwidth_khz != 250) return false;
  return true;
}

// --- Task 2: Validation and typed diagnostics ---

bool TestValidFullLoRaConfigAccepted() {
  LoRaDriver driver;
  return driver.begin(MakeV1LoRaConfig()) == LoRaError::kOk && driver.isInitialized();
}

bool TestSF7BoundaryAccepted() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.spreading_factor = 7;
  return driver.begin(config) == LoRaError::kOk;
}

bool TestSF12BoundaryAccepted() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.spreading_factor = 12;
  return driver.begin(config) == LoRaError::kOk;
}

bool TestSF6BelowRangeRejected() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.spreading_factor = 6;
  if (driver.begin(config) != LoRaError::kInvalidConfig) return false;
  if (driver.isInitialized()) return false;
  return driver.lastDiagnosticCode() == kDiagInvalidSpreadingFactor;
}

bool TestSF13AboveRangeRejected() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.spreading_factor = 13;
  if (driver.begin(config) != LoRaError::kInvalidConfig) return false;
  return driver.lastDiagnosticCode() == kDiagInvalidSpreadingFactor;
}

bool TestBW125Accepted() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.bandwidth_khz = 125;
  return driver.begin(config) == LoRaError::kOk;
}

bool TestBW250Accepted() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.bandwidth_khz = 250;
  return driver.begin(config) == LoRaError::kOk;
}

bool TestBW500Accepted() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.bandwidth_khz = 500;
  return driver.begin(config) == LoRaError::kOk;
}

bool TestUnsupportedBWRejected() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.bandwidth_khz = 200;
  if (driver.begin(config) != LoRaError::kInvalidConfig) return false;
  if (driver.isInitialized()) return false;
  return driver.lastDiagnosticCode() == kDiagInvalidBandwidth;
}

bool TestCR45Accepted() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.coding_rate_denominator = 5;
  return driver.begin(config) == LoRaError::kOk;
}

bool TestCR48Accepted() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.coding_rate_denominator = 8;
  return driver.begin(config) == LoRaError::kOk;
}

bool TestCRDenominator4Rejected() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.coding_rate_denominator = 4;
  if (driver.begin(config) != LoRaError::kInvalidConfig) return false;
  return driver.lastDiagnosticCode() == kDiagInvalidCodingRate;
}

bool TestCRDenominator9Rejected() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.coding_rate_denominator = 9;
  if (driver.begin(config) != LoRaError::kInvalidConfig) return false;
  return driver.lastDiagnosticCode() == kDiagInvalidCodingRate;
}

bool TestPreamble6Accepted() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.preamble_length = 6;
  return driver.begin(config) == LoRaError::kOk;
}

bool TestPreamble65535Accepted() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.preamble_length = 65535;
  return driver.begin(config) == LoRaError::kOk;
}

bool TestPreamble5Rejected() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.preamble_length = 5;
  if (driver.begin(config) != LoRaError::kInvalidConfig) return false;
  return driver.lastDiagnosticCode() == kDiagInvalidPreambleLength;
}

bool TestPreamble0Rejected() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.preamble_length = 0;
  if (driver.begin(config) != LoRaError::kInvalidConfig) return false;
  return driver.lastDiagnosticCode() == kDiagInvalidPreambleLength;
}

bool TestInvalidParamDetailCodesAreDistinct() {
  LoRaDriver d1;
  RadioConfig c1 = MakeV1LoRaConfig();
  c1.spreading_factor = 6;
  (void)d1.begin(c1);
  const int sf_code = d1.lastDiagnosticCode();

  LoRaDriver d2;
  RadioConfig c2 = MakeV1LoRaConfig();
  c2.bandwidth_khz = 200;
  (void)d2.begin(c2);
  const int bw_code = d2.lastDiagnosticCode();

  LoRaDriver d3;
  RadioConfig c3 = MakeV1LoRaConfig();
  c3.coding_rate_denominator = 4;
  (void)d3.begin(c3);
  const int cr_code = d3.lastDiagnosticCode();

  LoRaDriver d4;
  RadioConfig c4 = MakeV1LoRaConfig();
  c4.preamble_length = 0;
  (void)d4.begin(c4);
  const int pre_code = d4.lastDiagnosticCode();

  if (sf_code == bw_code) return false;
  if (sf_code == cr_code) return false;
  if (sf_code == pre_code) return false;
  if (bw_code == cr_code) return false;
  if (bw_code == pre_code) return false;
  if (cr_code == pre_code) return false;
  return true;
}

bool TestInvalidParamRejectedBeforeHardwareInit() {
  LoRaDriver driver;
  std::vector<RadioEvent> events;
  (void)driver.setEventCallback([&events](RadioEvent event, int) {
    events.push_back(event);
  });

  RadioConfig config = MakeV1LoRaConfig();
  config.spreading_factor = 6;
  (void)driver.begin(config);

  for (const auto& evt : events) {
    if (evt == RadioEvent::kInitialized) return false;
  }

  bool found_error = false;
  for (const auto& evt : events) {
    if (evt == RadioEvent::kError) {
      found_error = true;
      break;
    }
  }
  return found_error;
}

bool TestNoPartialStateAfterInvalidParam() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.bandwidth_khz = 99;
  (void)driver.begin(config);

  if (driver.isInitialized()) return false;
  return driver.lastError() == LoRaError::kInvalidConfig;
}

bool TestExistingProfileValidationPreserved() {
  LoRaDriver d1;
  RadioConfig c1 = MakeV1LoRaConfig();
  c1.chip = RadioConfig::Chip::kSx126xStub;
  if (d1.begin(c1) != LoRaError::kUnsupportedProfile) return false;

  LoRaDriver d2;
  RadioConfig c2 = MakeV1LoRaConfig();
  c2.band = static_cast<RadioConfig::Band>(99);
  if (d2.begin(c2) != LoRaError::kUnsupportedProfile) return false;

  LoRaDriver d3;
  RadioConfig c3 = MakeV1LoRaConfig();
  c3.spi_frequency_hz = 1000000;
  if (d3.begin(c3) != LoRaError::kInvalidConfig) return false;

  return true;
}

// --- Task 3: LoRa params in diagnostics and snapshots ---

bool TestSnapshotIncludesLoRaParams() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.spi_frequency_hz = 4000000;
  config.spreading_factor = 10;
  config.bandwidth_khz = 250;
  config.coding_rate_denominator = 6;
  config.sync_word = 0x34;
  config.tx_power_dbm = 20;
  config.crc_enabled = false;
  config.preamble_length = 16;

  if (driver.begin(config) != LoRaError::kOk) return false;

  const IncidentSnapshot snapshot = driver.captureIncidentSnapshot();
  if (snapshot.spi_frequency_hz != 4000000) return false;
  if (snapshot.spreading_factor != 10) return false;
  if (snapshot.bandwidth_khz != 250) return false;
  if (snapshot.coding_rate_denominator != 6) return false;
  if (snapshot.sync_word != 0x34) return false;
  if (snapshot.tx_power_dbm != 20) return false;
  if (snapshot.crc_enabled != false) return false;
  if (snapshot.preamble_length != 16) return false;
  return true;
}

bool TestSnapshotFormatIncludesLoRaParams() {
  LoRaDriver driver;
  if (driver.begin(MakeV1LoRaConfig()) != LoRaError::kOk) return false;

  const IncidentSnapshot snapshot = driver.captureIncidentSnapshot();
  char buffer[IncidentSnapshot::kFormatBufferSize];
  const std::size_t written = snapshot.formatTo(buffer, sizeof(buffer));
  if (written == 0) return false;

  const std::string output(buffer, written);
  if (!Contains(output, "spi=")) return false;
  if (!Contains(output, "sf=")) return false;
  if (!Contains(output, "bw=")) return false;
  if (!Contains(output, "cr=")) return false;
  if (!Contains(output, "sw=")) return false;
  if (!Contains(output, "pwr=")) return false;
  if (!Contains(output, "crc=")) return false;
  if (!Contains(output, "pre=")) return false;
  return true;
}

bool TestDiagnosticContextIncludesLoRaParams() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.spi_frequency_hz = 6000000;
  config.spreading_factor = 11;
  config.bandwidth_khz = 500;
  config.coding_rate_denominator = 7;
  config.preamble_length = 12;

  if (driver.begin(config) != LoRaError::kOk) return false;

  const auto ctx = driver.lastDiagnosticContext();
  if (ctx.spi_frequency_hz != 6000000) return false;
  if (ctx.spreading_factor != 11) return false;
  if (ctx.bandwidth_khz != 500) return false;
  if (ctx.coding_rate_denominator != 7) return false;
  if (ctx.preamble_length != 12) return false;
  return true;
}

bool TestSnapshotLoRaParamsSufficientForReproduction() {
  // Verify that two configs with different LoRa params produce snapshots that
  // can be distinguished - i.e., the snapshot contains enough for reproduction.
  LoRaDriver d1;
  RadioConfig c1 = MakeV1LoRaConfig();
  c1.spreading_factor = 7;
  c1.bandwidth_khz = 500;
  if (d1.begin(c1) != LoRaError::kOk) return false;
  const IncidentSnapshot s1 = d1.captureIncidentSnapshot();

  LoRaDriver d2;
  RadioConfig c2 = MakeV1LoRaConfig();
  c2.spreading_factor = 12;
  c2.bandwidth_khz = 125;
  if (d2.begin(c2) != LoRaError::kOk) return false;
  const IncidentSnapshot s2 = d2.captureIncidentSnapshot();

  if (s1.spreading_factor == s2.spreading_factor) return false;
  if (s1.bandwidth_khz == s2.bandwidth_khz) return false;
  return true;
}

bool TestSnapshotReflectsFailedBeginAttemptedConfig() {
  LoRaDriver driver;
  RadioConfig config = MakeV1LoRaConfig();
  config.spi_frequency_hz = 7000000;
  config.spreading_factor = 6;  // invalid
  config.bandwidth_khz = 250;
  config.coding_rate_denominator = 8;
  config.sync_word = 0x34;
  config.tx_power_dbm = 20;
  config.crc_enabled = false;
  config.preamble_length = 12;

  if (driver.begin(config) != LoRaError::kInvalidConfig) return false;

  const IncidentSnapshot snapshot = driver.captureIncidentSnapshot();
  if (snapshot.detail_code != kDiagInvalidSpreadingFactor) return false;
  if (snapshot.spi_frequency_hz != 7000000) return false;
  if (snapshot.spreading_factor != 6) return false;
  if (snapshot.bandwidth_khz != 250) return false;
  if (snapshot.coding_rate_denominator != 8) return false;
  if (snapshot.sync_word != 0x34) return false;
  if (snapshot.tx_power_dbm != 20) return false;
  if (snapshot.crc_enabled != false) return false;
  if (snapshot.preamble_length != 12) return false;
  return true;
}

#define RUN_TEST(fn) \
  if (!(fn)()) { \
    std::fprintf(stderr, "FAIL: %s\n", #fn); \
    return EXIT_FAILURE; \
  }

int RunLoRaConfigApiTests() {
  // Task 1: RadioConfig LoRa fields and defaults
  RUN_TEST(TestRadioConfigHasV1LoRaDefaults)
  RUN_TEST(TestDefaultRadioConfigBeginSucceeds)
  RUN_TEST(TestRadioConfigTrivialCopyPreserved)
  // Task 2: Validation and typed diagnostics
  RUN_TEST(TestValidFullLoRaConfigAccepted)
  RUN_TEST(TestSF7BoundaryAccepted)
  RUN_TEST(TestSF12BoundaryAccepted)
  RUN_TEST(TestSF6BelowRangeRejected)
  RUN_TEST(TestSF13AboveRangeRejected)
  RUN_TEST(TestBW125Accepted)
  RUN_TEST(TestBW250Accepted)
  RUN_TEST(TestBW500Accepted)
  RUN_TEST(TestUnsupportedBWRejected)
  RUN_TEST(TestCR45Accepted)
  RUN_TEST(TestCR48Accepted)
  RUN_TEST(TestCRDenominator4Rejected)
  RUN_TEST(TestCRDenominator9Rejected)
  RUN_TEST(TestPreamble6Accepted)
  RUN_TEST(TestPreamble65535Accepted)
  RUN_TEST(TestPreamble5Rejected)
  RUN_TEST(TestPreamble0Rejected)
  RUN_TEST(TestInvalidParamDetailCodesAreDistinct)
  RUN_TEST(TestInvalidParamRejectedBeforeHardwareInit)
  RUN_TEST(TestNoPartialStateAfterInvalidParam)
  RUN_TEST(TestExistingProfileValidationPreserved)
  // Task 3: LoRa params in diagnostics and snapshots
  RUN_TEST(TestSnapshotIncludesLoRaParams)
  RUN_TEST(TestSnapshotFormatIncludesLoRaParams)
  RUN_TEST(TestDiagnosticContextIncludesLoRaParams)
  RUN_TEST(TestSnapshotLoRaParamsSufficientForReproduction)
  RUN_TEST(TestSnapshotReflectsFailedBeginAttemptedConfig)
  return EXIT_SUCCESS;
}

}  // namespace

int main() {
  return RunLoRaConfigApiTests();
}
