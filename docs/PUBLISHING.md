# Publishing the Freenove FNK0104S beta fork

This is a maintainer procedure for the **unofficial** [Pocket Tank](https://github.com/mediacutlet/pocket-tank) compatibility fork. Repository publication, firmware distribution and flashing a physical board are **separate operator decisions**.

## First beta published (2026-11-16)

- Public fork: [XxYouDeaDPunKxX/pocket-tank-freenove-fnk0104s](https://github.com/XxYouDeaDPunKxX/pocket-tank-freenove-fnk0104s), based on upstream `c0d37ae` with one port commit `160b826`.
- [Live browser installer](https://xxyoudeadpunkxx.github.io/pocket-tank-freenove-fnk0104s/) is served from the separate `gh-pages` branch. The site includes the signed FNK0104S image, six flash parts, normal/erase manifests and the vendor ESP Web Tools assets.
- [Beta 0.4.2 prerelease](https://github.com/XxYouDeaDPunKxX/pocket-tank-freenove-fnk0104s/releases/tag/fnk0104s-v0.4.2-beta.1) carries the signed app image, OTA manifest and offline installer ZIP.
- The first public firmware was built and signed locally with ESP-IDF 5.4.2 using the persistent key that remains outside Git. **The private key has not been uploaded as an Actions secret**. The repository's Actions publication variable is not enabled, so `installer.yml` and `release.yml` remain gated. GitHub Pages currently uses **Deploy from a branch** (`gh-pages`, `/`) rather than the future **GitHub Actions** publishing source described below.

The remaining steps describe **future CI automation and later releases**; switching Pages to GitHub Actions should happen only after the Actions signing secret, gate and hosted build are verified. Do not replace the working `gh-pages` deployment prematurely.

## Repository and history

- Suggested repository name: `pocket-tank-freenove-fnk0104s`.
- Suggested description: `Unofficial Pocket Tank beta port for Freenove FNK0104S (ESP32-S3, 4.0-inch ST7796 480×320)`.
- GitHub's **Fork** feature preserves the upstream relationship and upstream Git history. Keep the inherited history and present **the entire FNK0104S port as one reviewed commit** on top of the original `v0.4.2` baseline (`c0d37ae`). The local experiment commits do not need to appear as separate commits on the eventual public branch.
- A repository with **literally one commit total** is a **new independent repository**, not a normal GitHub fork. Do not replace the fork's inherited history with a root-only commit or force-push over a different fork history. An ordinary fork with one port commit is the intended publication shape.
- Keep `upstream` pointing to `mediacutlet/pocket-tank` and attach the approved fork as `origin` only when authorized. A second GitHub Desktop clone of the existing local checkout is unnecessary.

Do not copy `firmware/keys/`, `firmware/build-freenove/`, `installer/dist/`, device save dumps, personal serial logs, toolchain caches or laboratory notes into the public commit. Include the original `LICENSE`, `NOTICE` and [upstream README](../README-UPSTREAM.md). The applicable license at this baseline is **PolyForm Noncommercial 1.0.0**; the historic MIT notices are retained in `NOTICE`.

## Before the publication gate

1. Review the exact **publication branch's** file inventory, commit diff and history. The port should be a single commit above the upstream `v0.4.2` baseline; validate its final tree against the locally tested working tree.
2. Build using **ESP-IDF 5.4.2** and a persistent RSA-3072 key; verify the signed image, FNK0104S board marker, partition layout, model trailer, installer manifests, `LICENSE` and `NOTICE`. Keep the private key outside Git and backed up securely.
3. Validate the local workflows and the beta URL generation. A successful local build does not prove the GitHub-hosted Pages and Release pipelines.
4. Confirm that the chosen GitHub account and repository name are final. The public Pages beta manifest address is constructed from those names at build time; renaming a published repository later can break installed devices' OTA channel.

## Publish only with separate approval

1. The operator creates the public fork of `mediacutlet/pocket-tank` using GitHub's **Fork** feature. Creating a fork of a public repository makes it publicly visible immediately.
2. Configure `origin` to that fork, keeping `upstream` unchanged. **Compare the fork's `main` with the port baseline before any push.** If the fork has newer upstream commits, replay the port changes as **one reviewed commit on top of that `main`** and rebuild before pushing. If the fork's `main` already contains the baseline and accepts a fast-forward, push the reviewed single-port-commit publication branch as its `main`. Do not push the experiment branch or force a non-fast-forward update.
3. In **Settings → Pages**, select **GitHub Actions** as the publishing source. If `github-pages` environment restrictions are enabled, allow deployments from both `main` **and beta release tags** matching `fnk0104s-v*-beta.*` (or use an equivalent reviewed environment rule). The beta release workflow also deploys Pages so the OTA manifest can advance.
4. Add the persistent private key to **Settings → Secrets and variables → Actions → Repository secrets** as `OTA_SIGNING_KEY`. Never paste it into issues, pull requests, logs, release notes or committed files. CI checks its public-key fingerprint against the key used by the tested FNK0104S.
5. Only when **installer and beta firmware distribution are separately approved**, add the Actions repository variable `ENABLE_FNK0104S_PUBLISH=true`. Until that exact value is present, both publishing workflows skip their jobs. Enabling it makes subsequent matching pushes, manual installer runs and matching beta tags capable of publishing.
6. Run `FNK0104S installer` on `main` and confirm GitHub Pages serves its installer, board-specific normal/erase manifests and six firmware parts, with HTTPS and the expected signed firmware version.
7. Create and push the beta tag **`fnk0104s-v0.4.2-beta.1`** only after the release is approved. `.github/workflows/release.yml` checks that `PT_RELEASE` is `0.4.2`, `PT_RELEASE_STAGE` is `beta` and the tag contains the matching numeric version. It publishes a **GitHub prerelease** (`--prerelease --latest=false`) with a signed FNK0104S `.bin`, `latest-fnk0104s.json`, an offline installer ZIP, `LICENSE` and `NOTICE`, then updates GitHub Pages with the beta channel manifest.
8. Verify on GitHub: installer download, board ID, SHA-256 and signature, published beta release, Pages manifest, and its signed image URL. Only then validate a real OTA update together with the operator, on a device already on the published beta channel.

## Beta OTA channel

The distributed beta image uses this **stable** manifest URL, derived from `GITHUB_REPOSITORY` in CI:

```text
https://<owner>.github.io/<repository>/latest-fnk0104s-beta.json
```

The manifest points to a signed beta image asset on the **same fork's GitHub Releases**, identified by its immutable version tag. The first beta is an **initial USB/browser installation baseline**: devices already running numeric `0.4.2` will not treat another `0.4.2` as a newer OTA update.

For the next beta OTA, increment `PT_RELEASE` **and** all matching numeric `PT_RELEASE_*` macros in `common/version.h` (for example, `0.4.3`), retain the signing key and board marker `fnk0104s`, and publish a new tag such as `fnk0104s-v0.4.3-beta.2`. The Pages manifest moves forward to that release. GitHub excludes prereleases from `releases/latest`, so **never** use `releases/latest/download` as the FNK0104S beta manifest URL.

The local `firmware/sdkconfig.freenove` intentionally keeps its URL empty; `tools/prepare_freenove_release.py` creates an ignored `sdkconfig.freenove.ci` in GitHub Actions. This prevents local test builds from silently tracking a release channel. A local USB-only image can still use NTP without OTA.

## References

- [GitHub forks](https://docs.github.com/en/pull-requests/reference/forks) and [fork creation](https://docs.github.com/en/pull-requests/how-tos/work-with-forks/fork-a-repo)
- [GitHub Pages with custom Actions](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages) and [deployment environment tags](https://docs.github.com/en/actions/reference/workflows-and-actions/deployments-and-environments)
- [GitHub Releases API: latest excludes prereleases](https://docs.github.com/en/rest/releases/releases#get-the-latest-release) and [GitHub CLI release creation](https://cli.github.com/manual/gh_release_create)
