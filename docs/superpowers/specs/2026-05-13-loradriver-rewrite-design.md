# LoRaDriver v1.0 — Refonte ciblée (SX1276 + SX1278)

**Date** : 2026-05-13
**Statut** : design validé, prêt pour writing-plans
**Cible repo** : `D:\DEV\C++\LoRaDriver` (remplace le squelette v0.1)
**Repo de référence** : `D:\DEV\C++\LoRaDriverBak` (v2.1.0, mature, déjà en prod)
**Consommateur de référence** : `D:\DEV\PlatformIO\SYNC-SIGNAL-LORA\SYNC-SIGNAL-LORA`

---

## 1. Contexte et motivation

Le repo `LoRaDriver` actuel (v0.1) est un harnais de gouvernance/FSM autour d'un moignon de driver : seule la séquence d'init registre SX1276 touche le silicium ; `send()`, `startReceive()`, `sleep()`, `standby()` n'écrivent rien au chip — ils enchaînent des transitions FSM et émettent des events synthétiques. Le SX1278 est "listé supporté" mais n'a aucune init hardware exécutée. Aucun chemin IRQ DIO0 réel n'existe.

À côté, le repo `LoRaDriverBak` (v2.1.0) est un driver mature ~2200 LoC déjà éprouvé en prod par SYNC-SIGNAL-LORA : SX127x + SX126x complets, FSM transceiver, IRQ queue, FreeRTOS pump task, errata Semtech, CAD, FHSS, LDRO, OCP, PA_BOOST/RFO, frequency error, RSSI/SNR, 60 tests host.

**Objectif** : gutter le repo `LoRaDriver`, y porter une vraie implémentation inspirée de Bak mais **strictement focalisée** sur SX1276 + SX1278, en respectant l'architecture clean attendue. Puis migrer SYNC-SIGNAL-LORA vers ce repo refondu.

---

## 2. Périmètre

### 2.1 Inclus

- Driver SX1276 + SX1278 complet (TX, RX, sleep, standby, CAD, runtime tuning).
- HAL SPI portable (`ISpiDevice`) + 3 implémentations : `ArduinoSpiDevice`, `Esp32SpiDevice` (DMA via `transferBytes`), `FakeSpiDevice` (tests host).
- Façade `LoRaTransceiver` (FSM + dispatch packet+meta).
- `RadioPumpTask` FreeRTOS ESP32 (ISR-driven, TX async, RX auto-restore).
- Errata Semtech : 2.1 (BW 500 kHz high-band) + 2.3 (RX spurious response) + sensitivity opt.
- Tests host : ~12 fichiers, ~30 tests, ~2 s CI.
- 1 test embedded smoke sur vrai ESP32 + module SX1276/78.
- 3 examples Arduino : `BasicSender`, `BasicReceiver`, `Esp32WithPumpTask`.
- Migration `SYNC-SIGNAL-LORA/src/lora_handler.cpp` vers la nouvelle lib.

### 2.2 Exclus (suppression nette)

- Toute la couche gouvernance/validation/CI : `src/validation/*`, `src/governance/*`, `tools/ci/gate_rules.yaml`.
- Toute la couche incident snapshot/diagnostic context (artefact gouvernance).
- SX126x (jamais utilisé par le consommateur, gros surface 1000+ LoC).
- Stubs `keep…ModuleLinked` (`src/core`, `src/infra`, `src/internal`, `src/platform/{arduino,esp32}/*_stub.cpp`, `src/chips/sx126x/`).
- Tests gouvernance (`test_{artifact_*,changelog_*,ci_gates,ota_gate,profile_qualification,release_monitoring,rollback_governance,traceability_engine,versioning_governance}`).
- `_bmad/`, `_bmad-output/`, `artifacts/`, `LoRaDriver.7z`.

### 2.3 Hors scope

- FHSS, RX duty cycle, ContinuousWave — pas demandés par le consommateur, à ajouter ultérieurement si besoin (pattern d'extension documenté).
- SX1277, SX1279 — même famille, ajoutables trivialement plus tard (changement de validation de bande uniquement).
- Cibles non-ESP32 pour la pump task (Arduino AVR, RP2040) : ces cibles ont `poll()` manuel, suffisant.

---

## 3. Architecture

### 3.1 Diagramme en couches

```
┌──────────────────────────────────────────────────┐
│  User code (SYNC-SIGNAL-LORA, sketches, etc.)    │
└────────────────┬─────────────────────────────────┘
                 │
        ┌────────▼──────────┐  loradriver::LoRaTransceiver
        │  Transceiver FSM  │   • mode control, send/receive sync
        │                   │   • dispatch packet+meta callback
        └────────┬──────────┘   • poll() pump events + watchdog
                 │
        ┌────────▼──────────┐  loradriver::IRadioDriver  (pure virtual)
        │  IRadioDriver     │
        └────────┬──────────┘
                 │
        ┌────────▼──────────┐  loradriver::chips::SX127xDriver
        │  SX127xDriver     │   • init seq, errata 2.1/2.3, LDRO auto,
        │  (1276 + 1278)    │     OCP, PA_BOOST/RFO, PaDac haute puiss.
        └────────┬──────────┘   • ring buffer IRQ, watchdog TX
                 │
        ┌────────▼──────────┐  loradriver::hal::ISpiDevice  (pure virtual)
        │  ISpiDevice       │
        └────────┬──────────┘
                 │
    ┌────────────┼────────────┐
    │            │            │
ArduinoSpi   Esp32Spi (DMA)  FakeSpi (tests host)

┌──────────────────────────────────────────────────┐
│  RadioPumpTask  (loradriver::platform::esp32)    │
│  [optional layer — ESP32 only]                   │
│   • FreeRTOS task pinned to core 1               │
│   • ISR notif → process_events → restore RX      │
│   • TX queue non-bloquante                       │
└──────────────────────────────────────────────────┘
```

### 3.2 Principes invariants

- Dependency injection partout, **aucun singleton**.
- Aucun `new` / `delete`, aucune allocation après `begin()`.
- Aucune exception (compile sous `-fno-exceptions`).
- Tout retour fallible : `[[nodiscard]] LoRaError noexcept`.
- ISR-safe : `volatile`, `IRAM_ATTR` (ESP32), pas de SPI en ISR par défaut.
- C++17 strict (utilise `std::optional`, `if constexpr`, structured bindings).
- Naming : `snake_case` pour méthodes/membres, `PascalCase` pour types/enums, `kPascalCase` pour constantes.

### 3.3 Layout final du repo

```
LoRaDriver/
├── CMakeLists.txt              # host build + ctest
├── platformio.ini              # esp32 build + smoke test
├── library.json / library.properties
├── README.md / CHANGELOG.md / LICENSE
├── include/loradriver/
│   ├── lora_config.hpp
│   ├── lora_error.hpp
│   ├── lora_packet.hpp
│   ├── radio_event.hpp
│   ├── radio_stats.hpp
│   ├── radio_driver.hpp         # IRadioDriver
│   ├── lora_transceiver.hpp
│   ├── hal/
│   │   ├── spi_device.hpp       # ISpiDevice
│   │   ├── arduino_spi_device.hpp
│   │   └── esp32_spi_device.hpp
│   ├── chips/
│   │   └── sx127x_driver.hpp
│   └── platform/
│       └── esp32/radio_pump_task.hpp
├── src/
│   ├── api/
│   │   ├── lora_config.cpp
│   │   ├── lora_error.cpp       # to_string()
│   │   └── lora_transceiver.cpp
│   ├── chips/sx127x/
│   │   ├── sx127x_driver.cpp
│   │   └── sx127x_registers.hpp # constexpr namespace, refs datasheet
│   ├── hal/
│   │   ├── spi_device.cpp       # default impl write/read_register
│   │   ├── arduino_spi_device.cpp  # #ifdef ARDUINO
│   │   └── esp32_spi_device.cpp    # #ifdef ARDUINO_ARCH_ESP32
│   └── platform/esp32/
│       └── radio_pump_task.cpp     # #ifdef ARDUINO_ARCH_ESP32
├── tests/
│   ├── host/
│   │   ├── CMakeLists.txt
│   │   ├── test_runner.hpp      # RUN_TEST + tiny assert
│   │   ├── fake_spi_device.hpp  # ISpiDevice + register sim + injectors
│   │   ├── test_lora_config_validate.cpp
│   │   ├── test_sx127x_init_sequence.cpp
│   │   ├── test_sx127x_tx_path.cpp
│   │   ├── test_sx127x_rx_path.cpp
│   │   ├── test_sx127x_irq_queue.cpp
│   │   ├── test_sx127x_errata.cpp
│   │   ├── test_sx127x_runtime_setters.cpp
│   │   ├── test_transceiver_fsm.cpp
│   │   ├── test_transceiver_callbacks.cpp
│   │   └── test_radio_stats.cpp
│   └── embedded/
│       └── smoke/test_main.cpp  # init real chip, echo loop
├── examples/
│   ├── BasicSender/
│   ├── BasicReceiver/
│   └── Esp32WithPumpTask/
└── docs/
    ├── api.md                   # public API reference
    ├── architecture.md          # schéma + flux 1 page
    ├── porting.md               # ajouter chip / plateforme
    ├── datasheet-refs.md        # liste sections Semtech utilisées
    └── superpowers/specs/       # ce spec + plans futurs
```

---

## 4. Composants détaillés

### 4.1 `LoRaConfig` — `include/loradriver/lora_config.hpp`

POD struct avec validation explicite.

```cpp
namespace loradriver {

enum class ChipModel : uint8_t { SX1276, SX1278 };
enum class PaOutput  : uint8_t { PaBoost, Rfo };

struct LoRaConfig {
    // RF
    uint32_t frequency_hz     = 868'000'000;
    uint8_t  spreading_factor = 9;          // 6..12 (SF6 ⇒ implicit_header)
    uint32_t bandwidth_hz     = 125'000;    // 7800..500000
    uint8_t  coding_rate      = 5;          // 5..8
    uint16_t preamble_length  = 8;          // 6..65535
    uint16_t symbol_timeout   = 100;        // RxSingle
    uint16_t sync_word        = 0x12;       // 0x12 priv, 0x34 LoRaWAN
    bool     crc_enabled      = true;
    bool     invert_iq        = false;
    bool     implicit_header  = false;

    // Power
    int8_t   tx_power_dbm     = 14;
    PaOutput pa_output        = PaOutput::PaBoost;
    uint8_t  ocp_ma           = 100;        // 45..240

    // Optimisations
    bool     ldro_auto        = true;
    bool     agc_auto         = true;
    bool     lna_boost_rx     = false;      // RegLna LnaBoostHf bits — sensibilité RX +3 dB
    bool     isr_snapshot     = false;      // si true: snapshot IRQ flags en ISR (1 SPI read ~5µs)

    // Chip + pinout
    ChipModel chip            = ChipModel::SX1276;
    uint32_t  spi_frequency_hz = 8'000'000;
    int8_t    pin_ss          = -1;          // required ≥ 0
    int8_t    pin_reset       = -1;          // required ≥ 0
    int8_t    pin_dio0        = -1;          // required ≥ 0
    int8_t    pin_dio1        = -1;          // optional

    [[nodiscard]] LoRaError validate() const noexcept;
    [[nodiscard]] bool      ldro_required() const noexcept;
};

}
```

`validate()` couvre :
- plage de fréquence selon `chip` : SX1278 = 137-525 MHz (rejette 868 MHz), SX1276 = 137-1020 MHz.
- BW : valeur dans `{7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000}`. BW 500 kHz interdite en low-band SX1278 (errata Semtech §2.3).
- SF dans `[6, 12]`. SF6 ⇒ `implicit_header` obligatoire.
- TX power vs PaOutput : `Rfo` ⇒ `[0, 14]`, `PaBoost` ⇒ `[2, 17]`, > 17 ⇒ activera `PaDac` (le validate accepte jusqu'à +20, le driver active le bit).
- OCP dans `[45, 240]`.
- coding rate `[5, 8]`, preamble ≥ 6, sync_word `[0, 0xFF]`.
- pins required ≥ 0.

### 4.2 `LoRaError` — `include/loradriver/lora_error.hpp`

```cpp
enum class LoRaError : uint8_t {
    OK = 0,
    InvalidConfig,
    UnsupportedChip,
    SpiFailure,
    SpiVerifyMismatch,
    InvalidState,
    TxTimeout,
    TxBufferTooLarge,
    RxTimeout,
    RxCrcError,
    AlreadyInitialized,
    NotInitialized,
    QueueFull,
    NullArgument,
};

const char* to_string(LoRaError e) noexcept;
```

Suppression des 12 codes gouvernance qui polluaient l'enum v0.1.

### 4.3 `RadioEvent` — `include/loradriver/radio_event.hpp`

```cpp
enum class RadioEvent : uint8_t {
    None = 0,
    TxDone,
    TxTimeout,
    RxDone,
    RxTimeout,
    RxCrcError,
    CadDone,
    CadDetected,
    ValidHeader,
    IrqOverflow,
};
```

### 4.4 `LoRaPacket` — `include/loradriver/lora_packet.hpp`

```cpp
struct LoRaPacket {
    int16_t  rssi_dbm        = 0;
    int16_t  snr_q4          = 0;          // SNR * 4 (entier)
    int32_t  frequency_error_hz = 0;
    uint8_t  length          = 0;
    bool     crc_valid       = false;

    [[nodiscard]] float snr_db() const noexcept { return snr_q4 / 4.0f; }
};
static_assert(sizeof(LoRaPacket) <= 16);
```

### 4.5 `RadioStats` — `include/loradriver/radio_stats.hpp`

```cpp
struct RadioStats {
    uint32_t tx_done = 0;
    uint32_t tx_timeout = 0;
    uint32_t rx_done = 0;
    uint32_t rx_timeout = 0;
    uint32_t rx_crc_errors = 0;
    uint32_t irq_events_processed = 0;
    uint32_t irq_overflows = 0;
    uint32_t callback_exceptions = 0;  // incrementé si un callback user throw (compilé -fexceptions)
    uint8_t  max_irq_backlog = 0;
    int16_t  last_rssi_dbm = 0;
    int16_t  last_snr_q4 = 0;
    int32_t  last_freq_error_hz = 0;
};
static_assert(std::is_trivially_copyable_v<RadioStats>);
```

### 4.6 `ISpiDevice` — `include/loradriver/hal/spi_device.hpp`

```cpp
namespace loradriver::hal {

class ISpiDevice {
public:
    virtual ~ISpiDevice() = default;
    virtual LoRaError begin() noexcept = 0;
    virtual LoRaError transfer(uint8_t addr,
                               const uint8_t* tx, uint8_t* rx,
                               size_t len) noexcept = 0;

    // Helpers (default impl via transfer())
    LoRaError write_register(uint8_t reg, uint8_t value) noexcept;
    LoRaError read_register(uint8_t reg, uint8_t& out) noexcept;
    LoRaError burst_write(uint8_t reg, const uint8_t* buf, size_t len) noexcept;
    LoRaError burst_read(uint8_t reg, uint8_t* buf, size_t len) noexcept;

protected:
    ISpiDevice() = default;
    ISpiDevice(const ISpiDevice&) = delete;
    ISpiDevice& operator=(const ISpiDevice&) = delete;
};

}
```

`addr` est passé tel quel ; le bit R/W (MSB) est positionné par les helpers selon l'opération. La méthode `transfer()` est responsable de claim/release du CS et de la transaction SPI atomique.

Implémentations :
- **`ArduinoSpiDevice`** : `SPI.transfer()` byte-par-byte, configurable bus et CS pin.
- **`Esp32SpiDevice`** : `spi.transferBytes(tx, rx, len)` (DMA). 5-10× plus rapide sur burst FIFO.
- **`FakeSpiDevice`** (tests) : array 256 octets, injecteurs `fail_next_read()`, `fail_next_write()`, `set_register(addr, val)`, `trigger_irq(flags)`.

### 4.7 `IRadioDriver` — `include/loradriver/radio_driver.hpp`

API extraite de Bak mais nettoyée et focalisée. Voir Section 2 du brainstorming pour la liste exhaustive de méthodes. Points clés :

- `begin(cfg)` / `end()` / `chip_version()`.
- `set_sleep()` / `set_standby()`.
- `start_transmit(data, len, timeout_ms)` + `is_transmitting()`.
- `start_receive(continuous)` + `read_packet(buf, max_len)`.
- `start_cad()`.
- Runtime setters : `set_frequency()`, `set_tx_power()`, `set_spreading_factor()`, `set_bandwidth()`.
- Metrics : `packet_rssi()`, `packet_snr()`, `frequency_error_hz()`, `current_rssi()`, `random_byte()`.
- `get_stats()` / `reset_stats()`.
- `set_event_callback(EventCallback)` (`std::function`) + variante raw `void(*)(void*, RadioEvent, int)` pour zéro alloc.
- `process_events()` (pump) + `handle_interrupt()` (ISR shim).

### 4.8 `SX127xDriver` — `src/chips/sx127x/sx127x_driver.cpp`

Une seule classe pour SX1276 et SX1278. La différence est gérée par :
- Validation de bande dans `LoRaConfig::validate()`.
- `set_tx_power(int8_t dbm, PaOutput out)` : si `dbm > 17` et `PaBoost`, écrit `RegPaDac = 0x87` + `RegOcp` ≥ 130 mA (datasheet §3.4.3).
- Offset RSSI selon bande : `-157` low-band, `-164` high-band (datasheet §5.5.5).

**Init séquence** (validée par `test_sx127x_init_sequence.cpp`) :
1. Reset GPIO : `RST↓ 1 ms, RST↑ 5 ms` (datasheet §7.2.2).
2. Lire `RegVersion (0x42)`, exiger `0x12`. Sinon `UnsupportedChip`.
3. `RegOpMode = FskSleep (0x00)` (prérequis pour switch LoRa).
4. `RegOpMode = LoRaSleep (0x80)`.
5. Verify : `RegOpMode & 0x80 != 0`. Sinon `SpiVerifyMismatch`.
6. Calculer FRF depuis `frequency_hz` (formule §6.4 : `frf = freq * 2^19 / 32_000_000`), écrire MSB/MID/LSB.
7. Verify FRF MSB.
8. `set_tx_power(cfg.tx_power_dbm, cfg.pa_output)`.
9. `ModemConfig1` : BW + CR + ImplicitHeader.
10. `ModemConfig2` : SF + CRC + SymbTimeout MSB.
11. `ModemConfig3` : **LDRO auto-calculé** (`ldro_required()` = symbol_duration > 16 ms) + `agc_auto`.
12. `RegSyncWord`.
13. `RegPreambleMsb/Lsb`.
14. `RegLna` : LnaGain max + LnaBoostHf=0b11 si `cfg.lna_boost_rx`.
15. `RegOcp` : enable + `ocp_ma` trim.
16. **Errata 2.1** (BW 500 kHz high-band) : `RegHighBwOptimize1/2` selon datasheet errata.
17. **Errata 2.3** (RX spurious) : `RegIfFreq1/2` selon BW.
18. `RegDioMapping1 = 0x00` (DIO0=RxDone par défaut).
19. `RegOpMode = LoRaStandby (0x81)`.
20. Clear IRQ flags (`RegIrqFlags = 0xFF`).

**TX path** :
- `set_standby()` (force STANDBY si pas déjà).
- `RegFifoTxBaseAddr = 0`, `RegFifoAddrPtr = 0`.
- `burst_write(RegFifo, data, len)` (DMA sur ESP32).
- `RegPayloadLength = len`.
- `RegDioMapping1 = TxDone << 6` via shadow check (évite SPI si déjà ok).
- `RegOpMode = TX (0x83)`.
- Set `tx_deadline_ms = now + timeout`, `tx_in_progress = true`.
- Retour OK immédiat ; le watchdog est piloté par `process_events()`.

**RX path** :
- `set_standby()`.
- `RegFifoRxBaseAddr = 0`, `RegFifoAddrPtr = 0`.
- `RegDioMapping1 = RxDone << 6`.
- `RegOpMode = RXCONTINUOUS (0x85)` ou `RXSINGLE (0x86)`.

**Read packet (depuis process_events)** :
- Lit snapshot : `RegFifoRxCurrentAddr`, `RegRxNbBytes`, `RegPktRssiValue`, `RegPktSnrValue`, `RegFeiMsb/Mid/Lsb`.
- `RegFifoAddrPtr = current_addr`.
- `burst_read(RegFifo, buf, nb_bytes)`.
- Clear IRQ flags.
- Formules (datasheet §5.5.5 et §4.1.5) :
  - `rssi_dbm = rssi_offset + raw + (snr_q4 < 0 ? snr_q4/4 : 0)` où `rssi_offset` = -157 (low-band) ou -164 (high-band).
  - `snr_q4 = (int8_t)raw_snr` (le registre est déjà en quarter-dB signé).
  - `freq_error_hz = (int32_t)fei_signed * 2^24 / 32_000_000 * (bw_hz / 500_000)` (formule §4.1.5).

**Ring buffer IRQ** :
- Capacité 16 entrées, chaque entrée = `{flags, rssi_raw, snr_raw, rx_addr, nb_bytes, timestamp_us}`.
- `handle_interrupt()` (IRAM_ATTR, ISR-safe) pousse une entrée. Si plein : `irq_overflows++`, drop oldest.
- Si `cfg.isr_snapshot = true` : lit `RegIrqFlags` en ISR (1 SPI court ~5 µs). Sinon : pousse une entrée vide et `process_events()` fera le SPI.
- `process_events()` : pop jusqu'à vide ou max 16 itérations, traite chaque entrée, met à jour stats, émet events.

**Shadows** :
- `_op_mode_shadow`, `_dio_mapping1_shadow` : évite des reads inutiles sur hot path.
- Invalidés sur erreur SPI, revalidés au prochain read.

**Verify policy** :
- Verify après init sur `OpMode`, `FRF MSB`, `ModemConfig1`, `SyncWord` (4 vérifs total au boot).
- Aucun verify sur le hot path TX/RX.

### 4.9 `LoRaTransceiver` — `src/api/lora_transceiver.cpp`

FSM façade :

```
UNINIT → begin() → STANDBY
STANDBY ↔ SLEEP (set_sleep / set_standby)
STANDBY → TX → STANDBY  (send() blocking ou start_transmit() async)
STANDBY → RX_CONTINUOUS (stays on RxDone)
STANDBY → RX_SINGLE → STANDBY (on RxDone/RxTimeout)
STANDBY → CAD → STANDBY ou RX_SINGLE (si autoRx)
Toute transition invalide → InvalidState
```

API :
- `begin(cfg)` : `validate()` → `driver.begin()` → register internal thunk → `STANDBY`.
- `send(data, len, timeout_ms = 2000)` : bloquant, retourne quand TxDone / TxTimeout / timeout.
- `start_transmit(data, len, timeout_ms)` : async, retourne immédiatement.
- `start_receive(continuous = true)`.
- `start_cad(auto_rx = false)`.
- `set_sleep()` / `set_standby()`.
- `on_receive(PacketCallback)` : `void(const LoRaPacket& meta, const uint8_t* data, size_t len)`.
- `on_event(EventCallback)` : `void(RadioEvent, int param)`.
- `on_tx_done(TxDoneCallback)` : `void()` — invoqué après TxDone.
- `poll()` : appelle `driver.process_events()`, traite watchdog, dispatch callbacks.
- `state()`, `rssi()`, `snr()`, `frequency_error_hz()`, `stats()`, `chip_version()`.

Buffer RX interne : `uint8_t _rx_buf[255]` (pas de heap).

Callbacks protégés par `try/catch(...)` (no-op si compilé `-fno-exceptions`), comptés via `stats.callback_exceptions`.

### 4.10 `RadioPumpTask` — `src/platform/esp32/radio_pump_task.cpp`

Repris de Bak avec améliorations :
- Queue TX configurable (défaut 4 au lieu de 1) pour absorber bursts.
- Métriques étendues : `tx_queue_depth_max`, `tx_enqueued`, `tx_errors`, `polls`, `max_poll_us`, `total_poll_us`.
- `on_tx_done()` callback exposé via le transceiver (au lieu de check `state == STANDBY`).
- `start()` accepte `priority`, `stack_words`, `core_id`, `period_ms`, `tx_queue_depth`.
- `stop()` propre : flush queue, vTaskDelete, libère ressources.

```cpp
class RadioPumpTask {
public:
    bool start(LoRaTransceiver& trx,
               uint32_t period_ms = 2,
               UBaseType_t priority = 2,
               uint32_t stack_words = 2048,
               BaseType_t core_id = 1,
               uint8_t tx_queue_depth = 4);
    void stop();
    bool running() const;
    bool enqueue_packet(const uint8_t* data, uint8_t len);
    void notify_from_isr();  // IRAM_ATTR

    struct Metrics { /* ... */ };
    Metrics metrics() const;
    void    reset_metrics();
};
```

---

## 5. Flux d'exécution

### 5.1 TX bloquant

```
trx.send(buf, len, 500ms)
 ├── FSM: si état ≠ STANDBY, set_standby()
 ├── driver.start_transmit(buf, len, 500)
 │    ├── set_mode(STANDBY)
 │    ├── write RegFifoTxBaseAddr=0, RegFifoAddrPtr=0
 │    ├── burst_write(RegFifo, buf, len)         ← DMA
 │    ├── write RegPayloadLength = len
 │    ├── ensure RegDioMapping1 = TxDone (shadow)
 │    ├── write RegOpMode = TX
 │    ├── tx_deadline_ms = now + 500
 │    └── return OK (non-bloquant)
 │
 └── while is_transmitting() && now < deadline:
       ├── driver.process_events()
       │    └── si TxDone IRQ : émet event, tx_in_progress=false
       └── delay(1) ou yield()
 → return OK | TxTimeout | SpiFailure
```

### 5.2 TX non-bloquant via pump task

```
user → pump.enqueue_packet(buf, len)  → xQueueSend, return
[pump_task]
  loop:
   ├── si !tx_pending && queue non vide:
   │    dequeue → trx.send(item.data, item.len, 500) → tx_pending=true
   ├── ulTaskNotifyTake(period_ms)        ← réveil ISR ou timeout
   ├── trx.poll()
   └── si tx_pending && state == STANDBY:
        tx_pending=false → trx.start_receive(true) → on_tx_done()
```

### 5.3 RX (continuous + IRQ)

```
[ISR DIO0 rising]  IRAM_ATTR  ~100 ns
 └── driver.handle_interrupt()
      ├── (si isr_snapshot) lit RegIrqFlags
      ├── push entrée dans ring[16]
      └── pump.notify_from_isr()

[pump_task] poll() → driver.process_events()
 ├── pop ring buffer
 ├── si RxDone:
 │    ├── snapshot (FifoRxCurrentAddr, RxNbBytes, PktRssi, PktSnr, Fei)
 │    ├── RegFifoAddrPtr = rx_addr
 │    ├── burst_read(RegFifo, buf, nb_bytes)
 │    ├── clear IRQ flags
 │    ├── update stats
 │    └── emit RadioEvent::RxDone
 └── transceiver:
      └── _packet_cb(meta, buf, nb_bytes)
```

### 5.4 Garanties d'exécution

- **Pas de SPI dans l'ISR** par défaut (`isr_snapshot = false`).
- Ring buffer borné à 16, drop oldest, compteur `irq_overflows`.
- `process_events()` borné à 16 itérations par appel (jamais d'inf loop).
- Watchdog TX dans `process_events()` : si `tx_deadline_ms` dépassé, force standby + émet `TxTimeout`.
- Watchdog RX (mode SINGLE) : `RegIrqFlags::RxTimeout` géré nativement.
- Aucune méthode publique ne laisse le chip dans un état indéterminé : sur erreur, `set_standby()` est appelé avant return.

---

## 6. Gestion des erreurs

### 6.1 Table

| Source | Détection | Action |
|---|---|---|
| Config invalide | `LoRaConfig::validate()` | `InvalidConfig`, aucune SPI émise |
| Chip absent / mauvais | `RegVersion != 0x12` | `UnsupportedChip`, driver reste UNINIT |
| SPI bus error | `ISpiDevice::transfer()` ≠ OK | `SpiFailure`, FSM → STANDBY |
| Read-back mismatch | check init (OpMode, FRF MSB, MC1, SyncWord) | `SpiVerifyMismatch` |
| TX timeout (DIO0 muet) | watchdog `tx_deadline_ms` | event `TxTimeout`, force standby |
| RX CRC error | `RegIrqFlags::PayloadCrcError` | stats++, event `RxCrcError`, no dispatch |
| IRQ ring full | en ISR | `irq_overflows++`, drop oldest, event `IrqOverflow` |
| Bad FSM state | check `_state` au début | `InvalidState`, aucun side-effect |
| Callback throws | `try/catch(...)` autour | swallow, stats compteur |
| Pump queue full | `xQueueSend` = pdFAIL | `QueueFull` au caller |

### 6.2 Récupération

- `set_standby()` toujours disponible (sauf UNINIT), idempotent.
- `end()` libère ISR, stoppe pump task, met chip en sleep.
- `begin()` peut être rappelé après échec sans `end()` préalable.

### 6.3 Thread safety

- `SX127xDriver` : safe pour `{ISR + 1 thread main}` ou `{ISR + pump task}`. Pas multi-writer.
- Membres lus en ISR : `volatile`.
- Compteurs incrémentés en ISR : `portENTER_CRITICAL(&mux)` sur ESP32.
- `LoRaTransceiver` : single-thread (pump task seul) ou guard externe.
- `RadioPumpTask` : sérialise toutes les ops radio via sa queue.

---

## 7. Tests

### 7.1 Tests host (CMake, ~2 s CI)

| Fichier | Couvre |
|---|---|
| `test_lora_config_validate.cpp` | Toutes branches de `validate()` : bande/chip, BW, SF, CR, power vs PaOutput, OCP, pins |
| `test_sx127x_init_sequence.cpp` | 20 étapes init, chaque failure point → diag code unique |
| `test_sx127x_tx_path.cpp` | Standby→TX, FIFO write, watchdog timeout, retour standby post-TxDone |
| `test_sx127x_rx_path.cpp` | Standby→RX, RxDone → read_packet, CRC valid/invalid, RSSI/SNR/freq_err calculation |
| `test_sx127x_irq_queue.cpp` | Ring buffer push/pop, overflow, ordre FIFO, max_backlog |
| `test_sx127x_errata.cpp` | Errata 2.1 (BW 500 high-band) et 2.3 (IfFreq selon BW) → registres bonnes valeurs |
| `test_sx127x_runtime_setters.cpp` | `set_frequency/sf/bw/power` sans reconfig complète, validation des bornes |
| `test_transceiver_fsm.cpp` | Matrice exhaustive `state × action → expected_state` ou `InvalidState` |
| `test_transceiver_callbacks.cpp` | on_receive/on_event/on_tx_done : invoqués, paramètres corrects, exception caught |
| `test_radio_stats.cpp` | Counters increment correct, trivially_copyable, getter atomique |

`FakeSpiDevice` (header-only dans `tests/host/fake_spi_device.hpp`) :
- Array `uint8_t regs[256]`.
- Injecteurs : `fail_next_read()`, `fail_next_write(n)`, `set_register(addr, val)`, `trigger_irq(flags)`, `set_chip_version(val)`, `latency_us(us)`.
- Comptage des opérations pour assertions.

### 7.2 Test embedded (PlatformIO, ESP32 + module réel)

`tests/embedded/smoke/test_main.cpp` :
- Init SX1276 868 MHz config par défaut.
- Lit RegVersion → assert `== 0x12`.
- Écrit RegSyncWord = 0x42, relit → assert match.
- Boucle : `send("ping", 4)` puis `start_receive()` puis vérifie réception sur 2ème ESP32 (loopback).
- Skippé si pas de hardware (env var `SKIP_EMBEDDED=1`).

### 7.3 Cibles de couverture

- 100 % des branches de `LoRaConfig::validate()`.
- 100 % des chemins d'erreur SPI dans `sx127x_driver.cpp`.
- 100 % des transitions FSM de `LoRaTransceiver`.
- Pas de cible numérique stricte sur le reste, mais chaque méthode publique a au moins 1 test happy path + 1 test error path.

---

## 8. Migration du consommateur

### 8.1 Fichiers SYNC-SIGNAL-LORA modifiés

- `platformio.ini` : `lib_deps` change `symlink://D:/DEV/C++/LoRaDriverBak` → `symlink://D:/DEV/C++/LoRaDriver`.
- `src/lora_handler.cpp` + `include/lora_handler.h` : noms identiques (LoRaTransceiver, SX127xDriver, LoRaConfig, RadioPumpTask, Esp32SpiDevice), juste préfixer `loradriver::` ou ajouter `using namespace loradriver;`. Le snake_case côté méthodes change (`startReceive` → `start_receive`, `setEventCallback` → `set_event_callback`, etc.) : adaptation triviale.
- Vérifier sur 2 panels physiques (COM3 + COM4) : init OK, RX OK, TX OK, PERF logs cohérents.

### 8.2 Compatibilité Bak

Le consommateur ne dépend plus de Bak. Bak peut être archivé / supprimé après validation sur hardware.

---

## 9. Versionnage et release

- Version : passer de `0.1.0` → `1.0.0` (refonte = breaking change majeur).
- `CHANGELOG.md` : section "1.0.0 — 2026-05-13" avec sections BREAKING, ADDED, REMOVED.
- `library.json` / `library.properties` : update version + description + keywords.

---

## 10. Hors scope (futur)

- FHSS, RX duty cycle, ContinuousWave : extensions documentées.
- SX1277/79 : ajout de bande dans validate, sinon identique.
- SX126x : repo séparé ou plugin si besoin.
- Cibles non-ESP32 pour pump task : abstraction `IRadioScheduler` à introduire le moment venu.
- LoRaWAN MAC : autre couche, hors scope driver.

---

## 11. Critères de succès

- [ ] Aucun fichier `*_stub.cpp` ne reste dans `src/`.
- [ ] Aucun fichier sous `src/{validation,governance}/` ne reste.
- [ ] `loradriver::LoRaTransceiver` peut envoyer et recevoir un paquet sur 2 ESP32 + 2 modules SX1276 réels (smoke test embedded).
- [ ] Tous les tests host passent (ctest output `100% tests passed`).
- [ ] SYNC-SIGNAL-LORA compile + flash + fonctionne sur panel1 et panel2 (RX/TX visible dans monitor série).
- [ ] `LoRaConfig::validate()` rejette `SX1278 + 868 MHz` (bug actuel corrigé).
- [ ] `LoRaConfig::validate()` rejette `BW 500 + SX1278 low-band`.
- [ ] LDRO est appliqué automatiquement à SF11/SF12 @ 125 kHz.
- [ ] PA_BOOST haute puissance (`PaDac`) s'active à `tx_power_dbm > 17`.
- [ ] Reset GPIO est exécuté avant chaque init.
- [ ] `RegVersion` est vérifié et retourne `UnsupportedChip` si module absent.
- [ ] Pas de heap après `begin()` (vérifié via outil ou audit visuel).
- [ ] Pas d'exception lancée en runtime (compile sous `-fno-exceptions`).
- [ ] Examples Arduino `BasicSender`/`BasicReceiver`/`Esp32WithPumpTask` compilent et fonctionnent.
