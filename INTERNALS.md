# hw-heltecv4 — internals

Maintainer reference for the Heltec WiFi LoRa 32 (V4) board HAL. The
[README](README.md) is the operator guide and pin map; this document is for
changing the board code without breaking the bring-up. It is self-authoritative.

## 1. What this straddle adds

A non-buildable spangap component (`idf_component_register`, no `app_main`). It
exports one hook symbol the buildable's generated dispatcher calls, and
publishes the board's hardware description as `kconfig:` values. Source layout:

```
esp-idf/
├── CMakeLists.txt        component registration (+ SPANGAP_CONDITIONAL_SRCS glob)
├── include/heltecv4.h    board API + BOARD_VEXT_* pin macros
└── src/heltecv4.cpp      Vext power rail + LoRa CS park
```

Everything here is new (a board contributes hardware, not protocol). The
subsystems:

- **Vext peripheral power rail** (`heltecv4PowerInit`) — drives the active-low
  power-enable gate on to bring up the board's external +3.3 V rail.
- **LoRa CS park** (`heltecv4PowerInit`) — parks the SX1262's CS line HIGH so
  the radio stays deselected until `loraInit()` claims the pin.

The board also injects its hardware description as `kconfig:` values in
`straddle.yaml`, consumed by the owning straddle / IDF when staged under
`--with`:

- **Memory** — `CONFIG_ESPTOOLPY_FLASHSIZE_16MB`, `CONFIG_SPIRAM_MODE_QUAD`
  (the S3R2's 2 MB PSRAM is quad, not octal).
- **LoRa SX1262** (consumed by [iface-lora](../iface-lora)) —
  `CONFIG_LORA_COUNT=1`, `CONFIG_LORA_SPI_HOST=2`, SCK 9 / MOSI 10 / MISO 11,
  CS 8, DIO1 14, BUSY 13, RST 12, `CONFIG_LORA0_TCXO_MV=1800`,
  `CONFIG_LORA0_DIO2_RF_SWITCH=y`, `CONFIG_LORA0_RADIO_SX1262=y`.

There is no `sdkconfig.defaults` here on purpose: a non-buildable straddle's
`sdkconfig.defaults` is ignored under `--with`, so every value that must survive
into the buildable lives in `kconfig:` instead.

## 2. Bring-up ordering

The single hook is declared in `straddle.yaml`:

```
start:  heltecv4Start   (always)
```

**`heltecv4Start` runs in the `start:` band, before `spangapInit()`.** It is
bare-hardware bring-up with no platform dependency: `heltecv4PowerInit` drives
the Vext rail and parks the radio CS, then returns. Two things make this a
`start:` hook rather than an `init:` hook:

1. **Vext rail HIGH early.** `BOARD_VEXT_CTRL_PIN` (GPIO 36) gates the board's
   external +3.3 V rail behind a P-MOSFET, **active-low** — driving the gate LOW
   (`BOARD_VEXT_ON_LEVEL = 0`) turns the rail ON. `heltecv4PowerInit` drives it
   and waits ~100 ms for the rail to settle. Bringing it up before the platform
   comes up means any rail-powered peripheral added later is live at boot.
2. **LoRa CS parked HIGH before `loraInit()`.** The SX1262 shares no bus with
   the flash, so there is no SD-mount race to lose (the way the T-Deck does on
   its shared FSPI bus). But `loraInit()` (in iface-lora) runs later and only
   then owns `CONFIG_LORA0_CS_PIN` — until then the radio's CS floats. Park it
   HIGH (deselected) at `start:` so the live SX1262 cannot drive MISO before its
   driver claims the pin.

The CS park is guarded by `#if defined(CONFIG_LORA0_CS_PIN)`, which is defined
only when iface-lora is staged; the Vext block is guarded by
`#if BOARD_VEXT_CTRL_PIN >= 0`. A build without LoRa simply skips the CS park.

## 3. Pitfalls

- **`heltecv4Start` before `spangapInit()`.** Vext bring-up and the CS park must
  precede the platform; this is the reason for the `start:` band. Don't reorder
  it into `init:`. The radio is live once Vext settles, so its CS must already be
  parked before any bus activity.
- **CS park before `loraInit()`.** Park the SX1262's CS HIGH at `start:` so the
  deselected radio stays off MISO until `loraInit()` owns the pin. Removing the
  park lets a live, unselected radio drive the bus line.
- **Vext is active-low.** Drive GPIO 36 **LOW** to enable the rail
  (`BOARD_VEXT_ON_LEVEL = 0`). Driving it high turns the external rail off.
- **Vext must be exempt from light-sleep pin isolation.**
  `CONFIG_PM_SLP_DISABLE_GPIO` switches every pin to its sleep config on
  light-sleep entry; an isolated GPIO 36 floats, the P-MOSFET gate rises and
  the rail cuts out mid-sleep — the OLED loses VCC on the first sleep entry
  (typically the moment `usb down` releases the console's no-sleep lock) and
  comes back uninitialised, i.e. permanently dark. `heltecv4PowerInit` calls
  `gpio_sleep_sel_dis(36)` so the pin keeps driving through sleep; any future
  board-owned output that must hold its level (rail gates, resets) needs the
  same exemption. tinylcd does the equivalent for its OLED reset pin.
- **The RF path runs through the FEM — an undriven FEM is a broken radio.**
  TX and RX both traverse the front-end module; with its rail or enable pins
  unconfigured the link works at best heavily attenuated. iface-lora owns the
  FEM (detection, switching, gain conversion — see its `lora_fem.h`); this
  straddle only publishes the pins. The detect sense reads the enable net's
  pull-up, which is powered from the FEM side — the rail must be up first,
  which femInit handles. All FEM pins are exempted from light-sleep isolation
  for the same reason Vext is: a radio listening across light sleep needs the
  FEM held in its RX state, not floating.
- **Quad PSRAM, not octal — watch internal-DRAM headroom.** The S3R2 carries
  only 2 MB of PSRAM in quad mode (`CONFIG_SPIRAM_MODE_QUAD`), unlike the
  T-Deck's 8 MB octal S3R8. The platform's "octal PSRAM" assumption does not
  hold here; internal DRAM/DMA is the scarce resource. Keep FreeRTOS sync objects
  and DMA buffers in internal DRAM and watch the WiFi/lwIP static RX pools — a
  growing `.bss` will starve them.
