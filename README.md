# Pocket Tank — Freenove FNK0104S (ESP32-S3, 4.0-inch, 480×320) 🐟

**Unofficial hardware port of [Pocket Tank by mediacutlet / StratoBuilds](https://github.com/mediacutlet/pocket-tank)** for the **Freenove FNK0104S**. It runs the original fish simulation and its local 14M-parameter AI model on this ESP32-S3 display, with its own touchscreen, audio and BOOT-key integration.

**Release channel: beta.** The first fork release is **0.4.2 beta 1** (`fnk0104s-v0.4.2-beta.1`). The original game version remains 0.4.2; beta status identifies this hardware port. This repository distributes **FNK0104S firmware only**. Owners of the original Waveshare boards should use the [upstream project and installer](https://pocketank.com/install/).

![Pocket Tank aquarium in the original simulator](docs/media/sim-tank.png)

## Install on Freenove FNK0104S

**[OPEN THE FNK0104S WEB INSTALLER](https://xxyoudeadpunkxx.github.io/pocket-tank-freenove-fnk0104s/)**

Open that page in desktop **Chrome or Edge** with the Freenove board connected by a **USB-C data cable**. Click **Install Pocket Tank** and select the board's serial port. The page runs ESP Web Tools directly in the browser; you do not need to install a separate flashing application. The normal button preserves the tank's NVS save partition. The separate **Erase the board and install fresh** button intentionally removes progress and Wi-Fi settings.

An **[offline installer ZIP](https://github.com/XxYouDeaDPunKxX/pocket-tank-freenove-fnk0104s/releases/tag/fnk0104s-v0.4.2-beta.1)** is also available with the beta release. To use the ZIP, serve its extracted files on `localhost` as described in the [installer instructions](installer/README.md); Web Serial requires HTTPS or localhost.

The installer includes a **signed** Freenove firmware, bootloader, partition table, local AI model, model trailer and OTA initialization data. **Do not flash images for Waveshare or other ESP32-S3 boards onto the FNK0104S.** See [installer instructions](installer/README.md) for troubleshooting, local testing and save precautions.

## What works on this board

| Feature | FNK0104S behavior |
| --- | --- |
| Display | ST7796 SPI, 480×320 landscape; panel color order corrected |
| Touch | FT6336U, including manual NORMAL/TURNED 180° orientation |
| Sound | ES8311 I²S audio with amplifier control |
| Game | Original AI, fish, shop, saves and interaction |
| Power | USB-C supply; **battery percentage and charging are not measured** |
| Sleep / wake | BOOT and selectable LEGACY, SCREEN, LIGHT, NEVER modes; wake behavior depends on mode |
| Clock | Wi-Fi NTP sync; **no onboard RTC or IMU detected** |
| Updates | Signed **FNK0104S beta OTA** from this fork's release channel, after an initial beta-channel installation |

The display, touch, sound, rotation, short sleep/wake, NTP and save recovery after power removal were exercised on a real USB-powered FNK0104S. Battery runtime and energy consumption have **not** been measured. The first beta is installed through the browser or USB; checking future beta OTA versions requires a newer signed release with a higher numeric version.

The tank's AI decisions happen **on the ESP32-S3**, without a cloud AI service. Wi-Fi is used when needed for time synchronization or to check/download updates; your saved Wi-Fi credentials remain on the device. The browser installer loads its firmware files from the fork's hosting service.

## Build locally

Install **ESP-IDF 5.4.2**. Keep an **RSA-3072 signing key** at `firmware/keys/ota_signing_key.pem` (gitignored), and do not replace it if you expect previously installed devices to accept later signed OTA updates.

From `firmware/`:

```sh
idf.py -B build-freenove "-DSDKCONFIG=build-freenove/sdkconfig" "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.freenove" build
```

Then, from the repository root:

```sh
python tools/make_installer.py --build-dir firmware/build-freenove
python -m http.server --directory installer/dist 8765
```

The local configuration is intentionally USB-first; CI creates an **ignored configuration overlay** containing the exact GitHub Pages beta OTA URL based on the published fork's owner and repository name. Local builds do not silently point at someone else's firmware channel.

For hardware pinout, signed-build requirements and limitations see [FNK0104S board guide](docs/board-freenove-fnk0104s.md). Maintainers can follow [release instructions](docs/PUBLISHING.md). The source also includes the [upstream README](README-UPSTREAM.md), simulator, model tooling and original Waveshare targets for reference and future upstream synchronisation.

## Credit, scope and license

**Original Pocket Tank:** [mediacutlet/pocket-tank](https://github.com/mediacutlet/pocket-tank), by StratoBuilds. This is an **unofficial compatibility fork**, not an official Freenove or upstream release. The aquarium, its AI model, assets and original core code are credited to the upstream authors. This fork adapts the hardware integration to the FNK0104S.

The current upstream code and this fork are distributed under the **[PolyForm Noncommercial License 1.0.0](LICENSE)**. Older original releases up to `v0.3.9.2` carried MIT terms as described in [NOTICE](NOTICE); those historical notices are preserved. **Commercial use is not granted by this fork.** See [CONTRIBUTING.md](CONTRIBUTING.md) for contributions specific to this board.
