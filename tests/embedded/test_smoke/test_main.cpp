#include <unity.h>

#include "loradriver/lora_driver.hpp"

namespace {

int g_callback_count = 0;
loradriver::RadioEvent g_last_event = loradriver::RadioEvent::kNone;

void on_radio_event(loradriver::RadioEvent event, int detail_code) {
  (void)detail_code;
  g_last_event = event;
  ++g_callback_count;
}

void test_initialize_supported_profile_sets_initialized_and_emits_event() {
  using loradriver::LoRaDriver;
  using loradriver::LoRaError;
  using loradriver::RadioConfig;
  using loradriver::RadioEvent;

  g_callback_count = 0;
  g_last_event = RadioEvent::kNone;

  LoRaDriver driver;
  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kOk),
                        static_cast<int>(driver.setEventCallback(on_radio_event)));

  RadioConfig config;
  config.chip = RadioConfig::Chip::kSx1276;
  config.band = RadioConfig::Band::k868;
  config.dio_routing = RadioConfig::DioRouting::kDio0Only;

  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kOk),
                        static_cast<int>(driver.initialize(config)));
  TEST_ASSERT_TRUE(driver.isInitialized());
  TEST_ASSERT_EQUAL_INT(1, g_callback_count);
  TEST_ASSERT_EQUAL_INT(static_cast<int>(RadioEvent::kInitialized), static_cast<int>(g_last_event));
}

void test_initialize_deferred_profile_is_rejected() {
  using loradriver::LoRaDriver;
  using loradriver::LoRaError;
  using loradriver::RadioConfig;

  LoRaDriver driver;
  RadioConfig deferred_config;
  deferred_config.chip = RadioConfig::Chip::kSx126xStub;
  deferred_config.band = RadioConfig::Band::k868;
  deferred_config.dio_routing = RadioConfig::DioRouting::kDio0Only;

  TEST_ASSERT_EQUAL_INT(static_cast<int>(LoRaError::kUnsupportedProfile),
                        static_cast<int>(driver.initialize(deferred_config)));
  TEST_ASSERT_FALSE(driver.isInitialized());
}

}  // namespace

void setup() {
  UNITY_BEGIN();
  RUN_TEST(test_initialize_supported_profile_sets_initialized_and_emits_event);
  RUN_TEST(test_initialize_deferred_profile_is_rejected);
  UNITY_END();
}

void loop() {}
