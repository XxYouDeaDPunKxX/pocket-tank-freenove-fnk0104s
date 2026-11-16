# Freenove FNK0104S — Pocket Tank hardware port

This board integration extends upstream Pocket Tank `v0.4.2` at commit
`c0d37ae`. The original aquarium simulation, model, assets, progression,
save persistence, and update safety mechanisms are retained.

## Hardware integration

| Component | FNK0104S port |
| --- | --- |
| LCD | ST7796 SPI2, native 320×480 mapped to 480×320 landscape |
| Color | BGR order; corrected for the Freenove panel |
| Rendering | Original 448×368 world resampled in DMA stripes |
| Touch | FT6336U at I2C address `0x38`; mapping follows screen rotation |
| Audio | ES8311 I2S with amplifier enable active-low |
| BOOT | GPIO0; screen/legacy sleep and double-press shortcut |
| Power | USB-C-only profile by default; battery estimate optional and unvalidated |
| Networking | Wi-Fi provisioning, NTP without requiring a PMIC, separate Freenove OTA board ID |

Display pins: MOSI11, SCLK12, CS10, DC46, BL45.
Touch/I2C pins: SDA16, SCL15, reset18, IRQ17 (polled).
Audio pins: MCLK4, BCLK5, WS7, DOUT8; amplifier GPIO1 (low = on).
Optional battery voltage divider: GPIO9 / ADC1_CH8.

The microSD pins are present on the board, but no separate SD-card game
feature was added: this port follows the features included in the original
Pocket Tank release.

## Build: ESP-IDF 5.4.2

1. Activate the ESP-IDF 5.4.2 environment (ESP32-S3 toolchain).
2. From `firmware/`, provide a persistent RSA-3072 signing key at
   `keys/ota_signing_key.pem`. One-time generation for a *new* installation:
   `python -m espsecure generate_signing_key --version 2 --scheme rsa3072 keys/ota_signing_key.pem`.
   **Do not replace an installed device's signing key casually:** it
   determines which future OTA images that device will trust.
3. Build:

   ```powershell
   idf.py -B build-freenove "-DSDKCONFIG=build-freenove/sdkconfig" "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.freenove" build
   ```

4. To assemble a **local** browser installer, run this from repository root:

   ```powershell
   python tools/make_installer.py --build-dir firmware/build-freenove
   ```

The application, bootloader, partition table, and OTA initialization are
build outputs under `firmware/build-freenove/`. The separate quantized model
`model/out/model_q4.bin` is required at flash offset `0x290000`.
The installer assembler uses the build's flash layout rather than guessed
offsets. The default NVS save partition must be preserved for updates.
No firmware is flashed by either command above.

## Operation and verification

The default `CONFIG_POCKET_TANK_FREENOVE_BATTERY_ADC=n` profile assumes
external USB-C power. It does not measure USB presence, charging, battery
percentage, current, or runtime. Enabling the optional GPIO9 ADC path yields
only a rough cell-voltage estimate; it has not been validated with a cell.
No external energy measurement was available in the functional trials.

`SCREEN NORMAL/TURNED` controls 0°/180° rotation manually; no IMU-driven
orientation is assumed. `fnksleep` supports `LEGACY`, `SCREEN`, `LIGHT`,
and `NEVER`. With a USB serial host connected, LIGHT can fall back to
SCREEN; with a charger the operator confirmed touch wake. Timer-based deep
sleep and wake were separately observed by serial diagnostics.

In USB-C functional testing (2026-11-15/16), the operator confirmed display
colors, touch, sound, menu gestures, manual rotation and short LEGACY
sleep/wake. Serial logs additionally confirmed timer wake, saved-tank
restoration across a complete power interruption, and NTP reacquisition.
These are **functional results**, not measurements of electrical power
consumption or unattended long-duration operation.

## Network updates: beta release channel

The board image uses marker `fnk0104s`, distinct from all upstream
Waveshare images. A **locally compiled** image using `sdkconfig.freenove`
keeps `CONFIG_POCKET_TANK_FREENOVE_OTA_MANIFEST_URL=""`: checking for
updates reports `NOT READY`, while NTP works independently. **Published
beta images** use an ignored `sdkconfig.freenove.ci` overlay generated in
CI with the exact fork-specific manifest URL:

`https://<owner>.github.io/<repository>/latest-fnk0104s-beta.json`

The **signed** beta app and `latest-fnk0104s.json` are attached to the
corresponding tagged GitHub prerelease. The Pages channel manifest points
to that asset and advances with subsequent beta releases. It requires
continuity with the signing key trusted by installed devices. Do not use
the upstream Waveshare URLs for this board. See the
[release guide](PUBLISHING.md).

The first beta tag is `fnk0104s-v0.4.2-beta.1`, with `PT_RELEASE=0.4.2`
and `PT_RELEASE_STAGE=beta`. It is an installation baseline:
devices already running 0.4.2 will not see it as a newer OTA update.
Future updates must increment `PT_RELEASE` and its numeric version macros,
then publish a new signed FNK0104S release.

## Upstream relationship

Freenove support is isolated behind `CONFIG_POCKET_TANK_FREENOVE`,
board-specific ports, and `sdkconfig.freenove`. The upstream
[Pocket Tank](https://github.com/mediacutlet/pocket-tank) repository remains
the source of the original game. Its hardware-specific guides for Waveshare
refer to different boards and are retained only for their own targets.
