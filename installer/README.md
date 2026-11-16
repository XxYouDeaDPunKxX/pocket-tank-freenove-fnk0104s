# Browser installer — Freenove FNK0104S

This is the browser installer for the **unofficial FNK0104S beta port** of [Pocket Tank](https://github.com/mediacutlet/pocket-tank). It installs **only** the Freenove ESP32-S3 4.0-inch (480×320 ST7796) profile. For Waveshare hardware, use [upstream's installer](https://pocketank.com/install/).

The public installer lives on **GitHub Pages for this fork** after the repository's publication gate is opened. Find its HTTPS URL in the repository's **Settings → Pages** or **Deployments → github-pages**. You can alternatively use the offline installer ZIP attached to the fork's [beta release](../../../releases).

## Install or update over USB-C

1. Use **Chrome or Edge on a desktop** and a USB-C cable that carries data. Connect the **FNK0104S** to the computer.
2. Open the fork's HTTPS installer (or locally host the release ZIP as explained below). The page shows the firmware version it will write.
3. Click **Install Pocket Tank**, choose the ESP32-S3 serial port, and complete the ESP Web Tools dialog. On some systems it is labelled **USB JTAG/serial debug unit**; on others it has a COM or ttyACM name.
4. Allow the board to restart into the aquarium. If desired, use its on-screen Wi-Fi setup to save a network for **clock synchronization and signed beta OTA updates**.

**Normal Install preserves the NVS save partition** containing fish progress and network settings. A separate **Erase the board and install fresh** button deliberately erases them. Back up important progress before a beta update when possible; interrupted writes or unexpected firmware failures are always possible.

If the port does not appear, close serial monitors, check the cable and USB port, and try holding **BOOT while reconnecting** to enter ESP32-S3 download mode. During SCREEN sleep, one touch can wake the display; other sleep modes may use BOOT.

## Host the offline ZIP locally

Extract the full beta release archive, including `index.html`, `manifest.json`, `manifest-erase.json`, `firmware/` and `vendor/`. In the extracted directory, run:

```sh
python -m http.server 8765
```

Open `http://localhost:8765` in Chrome or Edge. Web Serial requires **HTTPS or localhost**. Opening `index.html` as a `file://` document is not supported.

## Assemble a local installer (maintainers)

With **ESP-IDF 5.4.2** activated and the persistent RSA-3072 signing key at `firmware/keys/ota_signing_key.pem`, build from `firmware/`:

```sh
idf.py -B build-freenove "-DSDKCONFIG=build-freenove/sdkconfig" "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.freenove" build
```

Return to the repository root and generate the site:

```sh
python tools/make_installer.py --build-dir firmware/build-freenove
python -m http.server --directory installer/dist 8765
```

This creates `installer/dist/` (gitignored) containing the HTML page, the two installer manifests, patched vendored [ESP Web Tools](https://esphome.github.io/esp-web-tools/) assets, six flash parts, `LICENSE` and `NOTICE`. The six parts are the bootloader, partition table, signed app, quantized model, model trailer and OTA initialization data. The firmware partition offsets come from the build configuration; do not substitute guessed addresses.

The local `sdkconfig.freenove` intentionally leaves the OTA manifest URL empty. **Published beta firmware** instead uses an ignored CI overlay generated from `GITHUB_REPOSITORY`; its OTA manifest URL is:

```text
https://<owner>.github.io/<repository>/latest-fnk0104s-beta.json
```

Each approved `fnk0104s-v<firmware-version>-beta.<N>` prerelease publishes its signed app on GitHub Releases and refreshes this beta manifest on Pages. The installer workflow also retains the current beta manifest when refreshing the site. Beta releases are GitHub **prereleases**; they are deliberately not advertised as stable `releases/latest`.

The standard manifest uses `never_erase: true`; `tools/make_installer.py` patches the vendored install dialog so that it respects this setting. The generator checks that no normal installer part overlaps the NVS partition at `0x9000`–`0xF000`. The erase manifest remains a separate, explicit operation. This is the inherited upstream save-preservation mechanism, retained for the FNK0104S.

The repository's [publishing guide](../docs/PUBLISHING.md) describes the GitHub Actions gate, signing-key continuity, beta release and deployment checks. `installer.yml` and `release.yml` cannot publish until the repository owner enables the explicit publication variable. **Building the local installer does not publish or flash anything.**

## Upstream credit

The underlying installer technique, ESP Web Tools integration, preservation safeguards and game belong to the [original Pocket Tank project](https://github.com/mediacutlet/pocket-tank). This fork adds the Freenove-specific build, assets and beta release channel. See [LICENSE](../LICENSE), [NOTICE](../NOTICE) and the [original README](../README-UPSTREAM.md).
