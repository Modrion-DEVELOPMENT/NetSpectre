"""Install into temporary directories; never bootstrap tools or change the user's PATH."""
import hashlib
import os
from pathlib import Path
import platform
import pty
import select
import signal
import shutil
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(platform.system() == 'Darwin' and shutil.which('cmake'), 'Requires macOS and preinstalled build tools')
class InstallerTests(unittest.TestCase):
    def test_install_update_and_launch_from_another_directory(self):
        with tempfile.TemporaryDirectory(prefix='netspro-install-test-') as directory:
            # Verify quoting of spaces, apostrophes and shell metacharacters.
            prefix = Path(directory) / "NetSpectre user's $test"
            command = ['bash', str(ROOT / 'scripts/install-macos.sh'), '--prefix', str(prefix), '--no-path', '--skip-tools']
            result = subprocess.run(command, input='', text=True, capture_output=True, cwd='/tmp')
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            app = prefix / 'app'
            self.assertTrue((app / 'Netspro').is_file())
            self.assertTrue((app / 'catalog/homebrew.json').is_file())
            self.assertTrue((app / 'LICENSE').is_file())
            config = app / 'config.json'
            config.write_text('{"version":1,"theme":"dark","settings":{},"installer_test":true}')
            theme = app / 'themes/dark.json'
            theme.write_text(theme.read_text() + '\n')
            expected = {path: hashlib.sha256(path.read_bytes()).hexdigest() for path in (config, theme)}
            result = subprocess.run(command, input='', text=True, capture_output=True, cwd='/tmp')
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for path, digest in expected.items():
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), digest)
            for name in ('netspro', 'Netspro'):
                master, slave = pty.openpty()
                process = subprocess.Popen([str(prefix / 'bin' / name)], cwd='/tmp', stdin=slave, stdout=slave, stderr=slave, start_new_session=True)
                output = b''
                try:
                    deadline = time.monotonic() + 12
                    while time.monotonic() < deadline and b'Username:' not in output:
                        if select.select([master], [], [], 0.2)[0]:
                            output += os.read(master, 65536)
                    self.assertIn(b'Username:', output, output[-1500:])
                finally:
                    if process.poll() is None:
                        os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=5)
                    os.close(master)
                    os.close(slave)


if __name__ == '__main__':
    unittest.main()
