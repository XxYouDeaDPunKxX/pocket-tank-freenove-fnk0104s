#!/usr/bin/env python3
"""Generate an ignored FNK0104S build config with its fork-specific OTA URL.

The tracked sdkconfig.freenove stays USB-first/offline. GitHub Actions calls
this only after the publication gate is explicitly enabled.
"""

import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "firmware" / "sdkconfig.freenove"
OUTPUT = ROOT / "firmware" / "sdkconfig.freenove.ci"
SETTING = "CONFIG_POCKET_TANK_FREENOVE_OTA_MANIFEST_URL"


def prepare(repository: str, source: Path = SOURCE, output: Path = OUTPUT) -> str:
    if not re.fullmatch(r"[A-Za-z0-9](?:[A-Za-z0-9-]{0,37}[A-Za-z0-9])?/[A-Za-z0-9_.-]+", repository):
        raise ValueError("expected GitHub owner/repository with no URL or whitespace")
    owner, repo = repository.split("/", 1)
    # GitHub's releases/latest intentionally excludes prereleases. The beta
    # manifest is updated on Pages whenever a signed beta is published.
    url = f"https://{owner.lower()}.github.io/{repo}/latest-fnk0104s-beta.json"
    original = source.read_text(encoding="utf-8")
    lines = original.splitlines(keepends=True)
    indices = [i for i, line in enumerate(lines) if line.startswith(SETTING + "=")]
    if len(indices) != 1:
        raise ValueError(f"expected exactly one {SETTING} line in {source}")
    i = indices[0]
    newline = "\r\n" if lines[i].endswith("\r\n") else "\n"
    lines[i] = f'{SETTING}="{url}"{newline}'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("".join(lines), encoding="utf-8")
    return url


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repository", required=True, help="GitHub owner/repository")
    args = parser.parse_args()
    url = prepare(args.repository)
    print(f"FNK0104S release config -> {OUTPUT.name} ({url})")


if __name__ == "__main__":
    main()
