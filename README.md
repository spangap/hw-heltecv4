# hw-heltecv4

Buildable straddle that produces the reticulous device image for the **Heltec
WiFi LoRa 32 (V4)** — an ESP32-S3R2 board (2 MB *quad* PSRAM, 16 MB flash) with
one Semtech **SX1262** LoRa modem. Board reference:
<https://heltec.org/project/wifi-lora-32-v4/>.

It is the board-HAL + buildable analogue of [`hw-tdeck`](../hw-tdeck), trimmed to
this board. **First cut: LoRa + Vext only** — no on-device UI (the V4's OLED is
not wired), no GNSS, no SD card. Those peripherals exist on the board and can be
added later; for now this is a headless LoRa node.

## What lives here

- `straddle.yaml` — the manifest. `prefix: heltecv4`, `buildable:` block, the
  default-on `additional_installs` menu (rns + the iface-* transports + lxmf/nomad/
  maps + the spangap net/web stack), the `start: heltecv4Start` bring-up hook,
  and the board `kconfig:` pin map (the SX1262 on SPI2). No `spangap-lcd`.
- `esp-idf/main/heltecv4.{cpp,h}` — the entire board HAL: drive the **Vext**
  peripheral power rail on (GPIO 36, active-low) and park the LoRa CS line HIGH
  before `loraInit()` claims it. Exposes `heltecv4Start()`.
- `esp-idf/main/{CMakeLists.txt,idf_component.yml}` — main component wiring
  (`${SPANGAP_REQUIRES}` + radiolib) and the generated app_main hookup.
- `esp-idf/sdkconfig.defaults` — project identity + board flash/PSRAM
  (`SPIRAM_MODE_QUAD`, 16 MB) + the µReticulum-required toggles (C++ exceptions,
  mbedTLS HKDF, stack bumps). Pin maps live in `straddle.yaml`'s `kconfig:`, not
  here.
- `web-interface/` — the Quasar SPA shell (shared with the reticulous family).

## Pin map (Heltec WiFi LoRa 32 V4)

| signal | GPIO | | signal | GPIO |
|---|---|---|---|---|
| LoRa NSS/CS | 8 | | LoRa RST | 12 |
| LoRa SCK | 9 | | LoRa BUSY | 13 |
| LoRa MOSI | 10 | | LoRa DIO1 | 14 |
| LoRa MISO | 11 | | Vext ctrl | 36 (active-low) |

The SX1262 drives DIO2 as its own RF antenna switch; DIO3 supplies the 1.8 V
TCXO. LoRa runs on its own SPI bus (`SPI2`/FSPI), separate from the flash.

## Build & flash

```sh
spangap build --no-lcd        # LoRa-only headless image (no on-device UI)
spangap flash <port>          # signal the host monitor to flash over USB
```

`--no-lcd` drops `spangap-lcd`, which is otherwise pulled in transitively as a
default-on optional of the UI straddles. There is no LCD board HAL here, so the
launcher has nothing to drive — keep it out until the OLED is wired.

## Caveats

- **Quad, not octal, PSRAM.** The S3R2 carries 2 MB of PSRAM in *quad* mode, so
  this board breaks the platform's usual "octal PSRAM" assumption
  (`SPIRAM_MODE_QUAD` in `sdkconfig.defaults`). Watch internal-DRAM headroom for
  DMA/WiFi/lwIP.
- Pins are from the published V4 pin map; confirm against your board revision
  before trusting RF results.
