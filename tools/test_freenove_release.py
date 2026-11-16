"""Release-channel contract: public beta firmware must discover beta OTA."""

import tempfile
import unittest
from pathlib import Path

from prepare_freenove_release import prepare


class FreenoveReleaseConfigTests(unittest.TestCase):
    def test_beta_manifest_url_is_stable_and_fork_specific(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "sdkconfig.freenove"
            output = Path(directory) / "sdkconfig.freenove.ci"
            source.write_text('OTHER=y\nCONFIG_POCKET_TANK_FREENOVE_OTA_MANIFEST_URL=""\n')
            url = prepare("ExampleOwner/pocket-tank-freenove-fnk0104s", source, output)
            self.assertEqual(
                url,
                "https://exampleowner.github.io/pocket-tank-freenove-fnk0104s/"
                "latest-fnk0104s-beta.json",
            )
            self.assertIn(f'CONFIG_POCKET_TANK_FREENOVE_OTA_MANIFEST_URL="{url}"', output.read_text())
            self.assertEqual(source.read_text().splitlines()[1], 'CONFIG_POCKET_TANK_FREENOVE_OTA_MANIFEST_URL=""')

    def test_rejects_an_invalid_github_owner(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "sdkconfig.freenove"
            source.write_text('CONFIG_POCKET_TANK_FREENOVE_OTA_MANIFEST_URL=""\n')
            with self.assertRaises(ValueError):
                prepare("owner_with_underscore/fork", source, Path(directory) / "ci")


if __name__ == "__main__":
    unittest.main()
