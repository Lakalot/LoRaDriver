// Multi-instance example: two SX1276 modules on independent SPI buses.
// On ESP32, VSPI (default SPI) + HSPI = two hardware SPI buses.
#include <Arduino.h>
#include <SPI.h>

#include <LoRaDriver.h>

#include "loradriver/chips/sx127x_driver.hpp"
#include "loradriver/hal/esp32_spi_device.hpp"
#include "loradriver/lora_transceiver.hpp"

using namespace loradriver;

SPIClass SPI2(HSPI);

// Instance A — VSPI bus, CS=5
hal::Esp32SpiDevice  spiA(SPI, /*cs=*/5);
chips::SX127xDriver  drvA(spiA);
LoRaTransceiver      trxA(drvA);

// Instance B — HSPI bus, CS=15
hal::Esp32SpiDevice  spiB(SPI2, /*cs=*/15);
chips::SX127xDriver  drvB(spiB);
LoRaTransceiver      trxB(drvB);

void setup() {
    Serial.begin(115200);
    SPI.begin();
    SPI2.begin(14, 12, 13);  // HSPI: SCK=14, MISO=12, MOSI=13

    LoRaConfig cfgA;
    cfgA.chip = ChipModel::SX1276;
    cfgA.frequency_hz = 868'000'000u;
    cfgA.pin_ss = 5; cfgA.pin_reset = 4; cfgA.pin_dio0 = 26;
    trxA.begin(cfgA);

    LoRaConfig cfgB;
    cfgB.chip = ChipModel::SX1276;
    cfgB.frequency_hz = 868'200'000u;  // different channel
    cfgB.pin_ss = 15; cfgB.pin_reset = 17; cfgB.pin_dio0 = 16;
    trxB.begin(cfgB);

    Serial.printf("A version=%02X  B version=%02X\n",
                  trxA.chip_version(), trxB.chip_version());
}

void loop() {
    trxA.poll();
    trxB.poll();
    delay(2);
}
