"""Exercise the POSIX downloader UI in an isolated pseudo terminal (mock installers; no system changes)."""
import fcntl
import json
import os
import pty
import select
import shutil
import struct
import subprocess
import tempfile
import termios
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class DownloaderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix="netspro-downloader-test-")
        cls.binary = Path(cls.build.name) / "catalog"
        harness = Path(cls.build.name) / "harness.cpp"
        harness.write_text('''#include "downloader.h"
#include <iostream>
int main() {
    try { netspro::openDownloader("\\033[37m", "\\033[36m", "\\033[32m"); }
    catch (const std::exception& e) { std::cerr << e.what(); return 2; }
}
''')
        subprocess.run([shutil.which("c++"), "-std=c++14", "-Wall", "-Wextra", "-Wpedantic",
                        "-I" + str(ROOT / "src"), "-I" + str(ROOT / "third_party"),
                        str(harness), str(ROOT / "src/downloader.cpp"), str(ROOT / "src/download_service.cpp"), str(ROOT / "src/catalog_sources.cpp"), "-o",
                        str(cls.binary)], check=True)
        cls.gate = Path(cls.build.name) / "command-gate"
        gate_source = Path(cls.build.name) / "gate.cpp"
        gate_source.write_text('''#define main netsproApplicationMain
#include "main.cpp"
#undef main
int main(int argc, char**) {
    developer = argc > 1;
    dev_mode = false;
    activeTheme = {{"main", "white"}, {"accent", "cyan"}, {"input", "green"}};
    inputLine = "downloader";
    processInput();
}
''')
        subprocess.run([shutil.which("c++"), "-std=c++14", "-I" + str(ROOT / "src"),
                        "-I" + str(ROOT / "third_party"), str(gate_source),
                        str(ROOT / "src/downloader.cpp"), str(ROOT / "src/download_service.cpp"), str(ROOT / "src/catalog_sources.cpp"), "-o", str(cls.gate)],
                       check=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="netspro-catalog-data-")
        data = Path(self.directory.name)
        (data / 'catalog').mkdir()
        shutil.copy(ROOT / 'catalog/third_party.json', data / 'catalog/third_party.json')
        mock = data / 'mock'
        mock.mkdir()
        for name in ('python3', 'py', 'ollama', 'brew', 'winget'):
            executable = mock / name
            executable.write_text('#!/bin/sh\necho "MOCK ' + name + ' $*"\nexit 0\n')
            executable.chmod(0o700)
        curl = mock / 'curl'
        curl.write_text("#!/bin/sh\necho '<a href=\"/library/llama3.2:1b-q4_K_M\">'\n")
        curl.chmod(0o700)
        self.environment = dict(os.environ, PATH=str(mock) + os.pathsep + os.environ['PATH'])
        self.master, self.slave = pty.openpty()
        self.original = termios.tcgetattr(self.slave)
        self.resize(90, 30)
        self.process = subprocess.Popen([self.binary], stdin=self.slave, stdout=self.slave,
                                        stderr=self.slave, cwd=self.directory.name, env=self.environment)
        self.read_until("Page 1 / 6")

    def tearDown(self):
        if self.process.poll() is None:
            os.write(self.master, b'\x03')
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        os.close(self.master)
        os.close(self.slave)
        self.directory.cleanup()

    def resize(self, width, height):
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ, struct.pack("HHHH", height, width, 0, 0))

    def read_until(self, text):
        output = b""
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            ready, _, _ = select.select([self.master], [], [], 0.1)
            if ready:
                output += os.read(self.master, 65536)
                if text.encode() in output:
                    return output
        self.fail(f"Expected {text!r}; got {output[-1800:]!r}")

    def send(self, data, expected):
        # Drain earlier frames before checking the response to a new input batch.
        while select.select([self.master], [], [], 0.12)[0]:
            os.read(self.master, 65536)
        os.write(self.master, data)
        return self.read_until(expected)

    def assert_terminal_restored(self):
        restored = termios.tcgetattr(self.slave)
        original = self.original[:]
        # Darwin sets this kernel-managed flag when canonical input is restored.
        pending = getattr(termios, 'PENDIN', 0)
        restored[3] &= ~pending
        original[3] &= ~pending
        self.assertEqual(restored, original)

    def test_mouse_navigation_and_real_install(self):
        output = self.send(b'\x1b[<35;5;10M', '> Ollama')
        self.assertIn(b'\x1b[7m', output)
        self.send(b'\x1b[<0;5;10M', 'Version: latest')
        self.send(b'\r', 'Type INSTALL')
        self.assertTrue(termios.tcgetattr(self.slave)[3] & termios.ICANON)
        self.send(b'INSTALL\n', 'Press Enter to return')
        self.send(b'\n', 'Details')
        self.send(b'b', 'Page 1 / 6')
        self.send(b'\x1b[<0;38;2M', 'Page 2 / 6')
        self.send(b'\x1b[D', 'Page 1 / 6')
        self.send(b'\x1b[6~', 'Page 2 / 6')
        self.send(b'\x1b[5~', 'Page 1 / 6')

    def test_search_empty_results_cancel_and_resize(self):
        self.send(b'/NumPy', '1 matches')
        self.send(b'\r\r', 'Version: latest')
        self.send(b'b/' + b'\x7f' * 8 + b'zzzzzz', 'No matches.')
        self.send(b'\r\r', 'No matches.')
        self.send(b'/test\x1b', 'Search: zzzzzz')
        self.resize(60, 15)
        self.read_until('resize terminal')
        self.resize(90, 30)
        self.read_until('No matches.')
        self.send(b'q', '\x1b[?1049l')
        self.process.wait(timeout=2)
        self.assertEqual(self.process.returncode, 0)
        self.assert_terminal_restored()

    def test_all_30_entries_and_ctrl_c_restore(self):
        self.send(b'\x1b[B' * 29, '> aiohttp')
        self.send(b'\r', 'Version: latest')
        self.send(b'\x03', '\x1b[?1049l')
        self.process.wait(timeout=2)
        self.assert_terminal_restored()

    def test_noninteractive_input_rejected(self):
        result = subprocess.run([self.binary], input=b'', capture_output=True, cwd=self.directory.name)
        self.assertEqual(result.returncode, 2)
        self.assertIn(b'interactive terminal', result.stderr)

    def test_developer_command_gate(self):
        denied = subprocess.run([self.gate], input=b'', capture_output=True)
        self.assertIn(b'Access denied (DEV only)', denied.stdout)
        # A developer with dev_mode off reaches the UI's terminal check.
        allowed = subprocess.run([self.gate, '--developer'], input=b'', capture_output=True)
        self.assertNotIn(b'Access denied', allowed.stdout)
        self.assertIn(b'interactive terminal', allowed.stderr)

    def test_install_cancel_keeps_ui_usable(self):
        self.send(b'\r\r', 'Type INSTALL')
        self.send(b'\n', 'Installation cancelled.')
        self.send(b'\n', 'Details')
        self.send(b'b', 'Page 1 / 6')

    def test_live_variants_and_pull(self):
        self.send(b'/llama3.2', '1 matches')
        self.send(b'\r\r\r', 'llama3.2:1b-q4_K_M')
        self.send(b'\r\r', 'Type INSTALL')
        output = self.send(b'INSTALL\n', 'Press Enter to return')
        self.assertIn(b'MOCK ollama pull llama3.2:1b-q4_K_M', output)
        cache = Path(self.directory.name) / 'catalog/ollama-tags-llama3.2.json'
        self.assertEqual(json.loads(cache.read_text()), ['llama3.2:1b-q4_K_M'])
        self.send(b'\n', 'Details')

    def test_offline_tag_cache(self):
        data = Path(self.directory.name)
        (data / 'mock/curl').write_text('#!/bin/sh\nexit 7\n')
        (data / 'catalog/ollama-tags-llama3.2.json').write_text('["llama3.2:cached-q8_0"]')
        self.send(b'/llama3.2', '1 matches')
        output = self.send(b'\r\r\r', 'llama3.2:cached-q8_0')
        self.assertIn(b'Using offline tags', output)

    def test_failed_install_preserves_browser(self):
        (Path(self.directory.name) / 'mock/brew').write_text('#!/bin/sh\nif [ "$1" = "--version" ]; then exit 0; fi\nexit 19\n')
        self.send(b'\r\r', 'Type INSTALL')
        output = self.send(b'INSTALL\n', 'Press Enter to return')
        self.assertIn(b'Command failed (exit 19)', output)
        self.send(b'\n', 'Details')
        self.send(b'b', 'Page 1 / 6')

    def test_filters_keyboard_mouse_and_reset(self):
        self.send(b'f', 'Operating system: All')
        self.send(b'\r', 'Operating system: windows')
        self.send(b'\x1b[B\r', 'Standalone: Yes')
        self.send(b'\x1b', '2 matches')
        self.send(b'/NumPy', 'No matches.')
        self.send(b'\r', 'No matches.')
        self.send(b'\x1b[<0;5;3M', 'Operating system: windows')
        self.send(b'r', 'Operating system: All')
        self.send(b'\x1b', '1 matches')
        self.send(b'f', 'Source: All')
        self.send(b'\x1b[<0;8;11M', 'Source: homebrew')
        self.send(b'\x1b', 'No matches.')

    def test_homebrew_snapshot_install(self):
        self.send(b'q', '\x1b[?1049l')
        self.process.wait(timeout=2)
        shutil.copy(ROOT / 'catalog/homebrew.json', Path(self.directory.name) / 'catalog/homebrew.json')
        self.process = subprocess.Popen([self.binary], stdin=self.slave, stdout=self.slave,
                                        stderr=self.slave, cwd=self.directory.name, env=self.environment)
        self.read_until('Page 1 / 39')
        self.send(b'/Homebrew:', '200 matches')
        self.send(b'\r\r\r', 'Type INSTALL')
        output = self.send(b'INSTALL\n', 'Press Enter to return')
        self.assertIn(b'MOCK brew install --formula', output)
        self.assertIn(b'Installation completed successfully.', output)
        self.send(b'\n', 'Details')


if __name__ == '__main__':
    unittest.main()
