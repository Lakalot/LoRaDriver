#include <unity.h>

#include <array>
#include <cstdint>

#include "loradriver/lora_driver.hpp"

namespace {

int g_callback_count = 0;
loradriver::RadioEvent g_last_event = loradriver::RadioEvent::kNone;
int g_last_detail = 0;

void on_radio_event(loradriver::RadioEvent event, int detail_code) {
  g_last_event = event;
  g_last_detail = detail_code;
  ++g_callback_count;
}

void test_begin_supported_profile_sets_initialized_and_emits_ready_event() {
  using loradriver::LoRaDriver;
  using loradriver::LoRaError;
  using loradriver::RadioConfig;
  using loradriver::RadioEvent;

  g_callback_count = 0;
  g_last_event = RadioEvent::kNone;
  g_last_detail = 0;

  LoRaDriver driver;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kOk),
                        static_cast<int>(driver.setEventCallback(on_radio_event)));

  RadioConfig config;
  config.chip = RadioConfig::Chip::kSx1276;
  config.band = RadioConfig::Band::k868;
  config.dio_routing = RadioConfig::DioRouting::kDio0Only;
  config.spi_frequency_hz = 8000000;

  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kOk),
                        static_cast<int>(driver.begin(config)));
  TEST_ASSERT_TRUE(driver.isInitialized());
  TEST_ASSERT_EQUAL_INT(4, g_callback_count);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RadioEvent::kInitialized), static_cast<int>(g_last_event));
  TEST_ASSERT_EQUAL_INT(0, g_last_detail);

  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kAlreadyInitialized),
                        static_cast<int>(driver.begin(config)));
  TEST_ASSERT_TRUE(driver.isInitialized());
}

void test_begin_deferred_profile_is_rejected_with_diagnostics() {
  using loradriver::LoRaDriver;
  using loradriver::LoRaError;
  using loradriver::RadioConfig;

  LoRaDriver driver;
  RadioConfig deferred_config;
  deferred_config.chip = RadioConfig::Chip::kSx126xStub;
  deferred_config.band = RadioConfig::Band::k868;
  deferred_config.dio_routing = RadioConfig::DioRouting::kDio0Only;
  deferred_config.spi_frequency_hz = 8000000;

  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kUnsupportedProfile),
                        static_cast<int>(driver.begin(deferred_config)));
  TEST_ASSERT_FALSE(driver.isInitialized());
  TEST_ASSERT_NOT_EQUAL(0, driver.lastDiagnosticCode());
  const auto context = driver.lastDiagnosticContext();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kUnsupportedProfile), static_cast<int>(context.error));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(deferred_config.chip), static_cast<int>(context.chip));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(deferred_config.band), static_cast<int>(context.band));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(deferred_config.dio_routing), static_cast<int>(context.dio_routing));
}

void test_begin_rejects_out_of_range_spi_without_mutating_input() {
  using loradriver::LoRaDriver;
  using loradriver::LoRaError;
  using loradriver::RadioConfig;

  LoRaDriver driver;
  RadioConfig config;
  config.chip = RadioConfig::Chip::kSx1278;
  config.band = RadioConfig::Band::k433;
  config.dio_routing = RadioConfig::DioRouting::kDio0Dio1;
  config.spi_frequency_hz = 9000000;

  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kInvalidConfig),
                        static_cast<int>(driver.begin(config)));
  TEST_ASSERT_EQUAL_UINT32(9000000u, config.spi_frequency_hz);
}

void test_begin_accepts_spi_boundaries() {
  using loradriver::LoRaDriver;
  using loradriver::LoRaError;
  using loradriver::RadioConfig;

  LoRaDriver driver;
  RadioConfig lower;
  lower.chip = RadioConfig::Chip::kSx1276;
  lower.band = RadioConfig::Band::k433;
  lower.dio_routing = RadioConfig::DioRouting::kDio0Only;
  lower.spi_frequency_hz = 4000000;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kOk), static_cast<int>(driver.begin(lower)));

  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kOk), static_cast<int>(driver.shutdown()));

  RadioConfig upper = lower;
  upper.spi_frequency_hz = 8000000;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kOk), static_cast<int>(driver.begin(upper)));
}

void test_shutdown_before_begin_returns_not_initialized_with_diagnostics() {
  using loradriver::LoRaDriver;
  using loradriver::LoRaError;

  LoRaDriver driver;
  TEST_ASSERT_FALSE(driver.isInitialized());
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kNotInitialized),
                        static_cast<int>(driver.shutdown()));
  const auto context = driver.lastDiagnosticContext();
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kNotInitialized),
                        static_cast<int>(context.error));
}

void test_send_and_receive_emit_contract_level_events() {
  using loradriver::LoRaDriver;
  using loradriver::LoRaError;
  using loradriver::RadioConfig;
  using loradriver::RadioEvent;

  g_callback_count = 0;
  g_last_event = RadioEvent::kNone;
  g_last_detail = 0;

  LoRaDriver driver;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kOk),
                        static_cast<int>(driver.setEventCallback(on_radio_event)));

  RadioConfig config;
  config.chip = RadioConfig::Chip::kSx1276;
  config.band = RadioConfig::Band::k868;
  config.dio_routing = RadioConfig::DioRouting::kDio0Only;
  config.spi_frequency_hz = 8000000;

  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kOk), static_cast<int>(driver.begin(config)));

  const std::array<std::uint8_t, 3> payload = {0x01u, 0x02u, 0x03u};
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kOk),
                        static_cast<int>(driver.send(payload.data(), payload.size())));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RadioEvent::kTxCompleted), static_cast<int>(g_last_event));

  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kOk),
                        static_cast<int>(driver.startReceive()));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RadioEvent::kRxDone), static_cast<int>(g_last_event));
}

void test_send_rejects_null_payload_with_typed_error() {
  using loradriver::LoRaDriver;
  using loradriver::LoRaError;
  using loradriver::RadioConfig;

  LoRaDriver driver;
  RadioConfig config;
  config.chip = RadioConfig::Chip::kSx1276;
  config.band = RadioConfig::Band::k868;
  config.dio_routing = RadioConfig::DioRouting::kDio0Dio1;
  config.spi_frequency_hz = 8000000;

  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kOk), static_cast<int>(driver.begin(config)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kInvalidConfig),
                        static_cast<int>(driver.send(nullptr, 1)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kInvalidConfig),
                        static_cast<int>(driver.lastDiagnosticContext().error));
}

}  // namespace

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_begin_supported_profile_sets_initialized_and_emits_ready_event);
  RUN_TEST(test_begin_deferred_profile_is_rejected_with_diagnostics);
  RUN_TEST(test_begin_rejects_out_of_range_spi_without_mutating_input);
  RUN_TEST(test_begin_accepts_spi_boundaries);
  RUN_TEST(test_shutdown_before_begin_returns_not_initialized_with_diagnostics);
  RUN_TEST(test_send_and_receive_emit_contract_level_events);
  RUN_TEST(test_send_rejects_null_payload_with_typed_error);
  UNITY_END();
}

void loop() {}
