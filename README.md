# hw-heltecv4 — Heltec WiFi LoRa 32 (V4) board HAL

**hw-heltecv4** is the board-support straddle for the **Heltec WiFi LoRa 32
(V4)** — an ESP32-S3R2 (16 MB flash, 2 MB **quad** PSRAM) carrying one Semtech
**SX1262** LoRa modem on its own SPI bus, behind a Vext-gated peripheral power
rail. It makes the board usable by an application: it owns the Vext rail
bring-up and the LoRa CS park, and it publishes the board's pin map and hardware
tuning as Kconfig. Board reference: <https://heltec.org/project/wifi-lora-32-v4/>.

It is a **non-buildable** component — it decides nothing about what the device
*does*. A buildable assembler (`reticulous/reticulous`) adds it and inherits the
board: `spangap build reticulous/reticulous --with spangap/hw-heltecv4`. The
mesh stack, the IP/web platform, `app_main`, the partition layout, the update
story and the browser SPA all come from the buildable and its other straddles —
not from here.

This board has no on-device UI, GNSS or SD card wired: it builds **headless**
automatically (there is no LCD straddle in its dependency set, so nothing pulls
in `spangap-lcd`). The V4's OLED, GNSS header and other peripherals exist on the
hardware but are deliberately left unwired here; only LoRa and the Vext rail are
brought up.

## Origins

The board is Heltec's WiFi LoRa 32 (V4). Pin assignments follow the published V4
pin map. The earlier V3 is a different module (ESP32-S3 without the R2 PSRAM
package) and is **not** this straddle; confirm pins against your board revision
before trusting RF results.

## What it does, and how it fits

The board contributes one hook that the buildable's generated init dispatcher
calls. There is nothing to call by hand: if the straddle is in the build, the
board comes up automatically.

| Hook | Band | Present when | Brings up |
|---|---|---|---|
| `heltecv4Start` | start | always | Vext peripheral power rail ON, LoRa CS park HIGH |

`heltecv4Start` runs in the `start:` band, **before** `spangapInit()`. It is
bare-hardware bring-up: it drives the Vext rail on so any rail-powered
peripheral is live at boot, and parks the SX1262's CS line HIGH so the radio
does not drive MISO before `loraInit()` (in [iface-lora](../iface-lora)) claims
the pin. There is no `init:`-band companion — there is no on-device UI in this
build.

Unlike the T-Deck, the SX1262 sits on its **own** SPI bus (separate from the
flash bus) and is powered directly rather than off Vext, so there is no
shared-bus SD probe to race during bring-up. The CS park is still required so
the deselected radio stays off the bus until its driver owns the pin.

The LoRa radio engine, the IP/web platform and the mesh stack are owned by other
straddles ([iface-lora](../iface-lora), [spangap-core](../spangap-core),
[spangap-net](../spangap-net), [rns](../rns)); this board only supplies the
SX1262's pins (below) and the power-rail/CS glue. The install stack those
straddles form is assembled by the buildable's `straddle.yaml`, not here.

## Hardware & pin map

Heltec WiFi LoRa 32 (V4) — **ESP32-S3R2** (16 MB flash, 2 MB **quad** PSRAM,
selected via `CONFIG_SPIRAM_MODE_QUAD`). A single SX1262 LoRa modem sits on
**SPI host 2** on its own bus, separate from the flash bus.

### LoRa SX1262 (owned by iface-lora, pins published here)

| Signal | GPIO | | Signal | GPIO |
|---|---|---|---|---|
| NSS / CS | 8 | | RST | 12 |
| SCK | 9 | | BUSY | 13 |
| MOSI | 10 | | DIO1 | 14 |
| MISO | 11 | | | |

The SX1262 drives **DIO2** as its own RF antenna switch and **DIO3** supplies
the 1.8 V TCXO (`CONFIG_LORA0_TCXO_MV=1800`). One radio
(`CONFIG_LORA_COUNT=1`), `CONFIG_LORA0_RADIO_SX1262=y`.

### Board-owned pin (in this straddle's `heltecv4.h`)

| Signal | GPIO | Notes |
|---|---|---|
| Vext peripheral power EN | 36 | **active-low** — drive LOW to enable the +3.3 V rail (OLED, GNSS header) |

### Memory / flash (published from `kconfig:`)

A non-buildable straddle has no `sdkconfig.defaults` of its own — it would be
ignored under `--with` — so every value that describes this hardware is
published from `straddle.yaml`'s `kconfig:` block and consumed by the owning
straddle / IDF:

| Key | Value | Why |
|---|---|---|
| `CONFIG_ESPTOOLPY_FLASHSIZE_16MB` | `y` | 16 MB flash |
| `CONFIG_SPANGAP_MAX_FIRMWARE_KB` | `6144` | state floor at 6 MB: `app`+`fixed` (~2.8 MB) plus growth headroom sit below it, and the runtime `/state` partition fills the remaining ~10 MB of the chip. Without it the floor defaults to the whole container and `app` eats all 16 MB — leaving **no `/state`** |
| `CONFIG_SPIRAM_MODE_QUAD` | `y` | the S3R2 carries 2 MB PSRAM in **quad** mode, not octal |

The platform's usual "octal PSRAM" assumption (the T-Deck's S3R8) does **not**
hold here — watch internal-DRAM headroom for DMA/WiFi/lwIP (see
[INTERNALS.md](INTERNALS.md)).

## Storage variables

This board defines no storage keys of its own. Runtime LoRa parameters live at
`s.lora.*` ([iface-lora](../iface-lora)).

## Dependencies

- [spangap-core](../spangap-core) — base runtime (storage, log, CLI, fs, ITS).
- [iface-lora](../iface-lora) — owns the SX1262 radio engine; this board parks
  its CS and supplies its pins via Kconfig.

## Read next

- [INTERNALS.md](INTERNALS.md) — the Vext/CS-park bring-up, the start-band
  ordering rule, and the board pitfalls.
