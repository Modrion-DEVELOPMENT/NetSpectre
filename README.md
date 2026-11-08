# Modrion NetSpectre

**A terminal application with themes, a searchable package catalog, and Ollama model downloads.**

Also known as **Nets** or **Netspro**. Written in C++14 by **Hendrik Hanking and the Modrion Intelligence Developer Team**.

[Install](#install-in-a-few-steps) · [Use](#first-start) · [Package catalog](#package-catalog) · [Build](#build-from-source) · [Contribute](#development-and-tests) · [License](#license-and-credits)

## Install in a few steps

Download the repository using **Code → Download ZIP**, then **extract the entire ZIP**.
Keep the `src`, `scripts`, `catalog`, `themes` and `third_party` folders together with the installer.
You do not need Git to install from a ZIP.

| Your system | Double-click | What happens |
| --- | --- | --- |
| macOS | **`install.command`** | Checks Apple developer tools, installs Homebrew/CMake if needed, builds NetSpectre and sets up the terminal command. |
| Windows 10/11 | **`install.bat`** | Installs WinGet if missing, installs CMake and Visual Studio C++ Build Tools if needed, builds NetSpectre and sets up the terminal command. |

1. Run the installer for your system as your **normal user**.
2. Follow the on-screen instructions. Approve system installation dialogs when asked.
3. Wait for **Installed successfully**. First-time developer tool downloads can take a while.
4. Close and reopen Terminal, Command Prompt or Windows Terminal.
5. Type either command:

```text
netspro
```

```text
Netspro
```

**Internet access is needed for missing tools.** Allow several GB of free disk space for
Windows C++ Build Tools. Windows 10 build 19041 or later is required; Windows 11 is recommended.
macOS must satisfy the current [Apple/Homebrew requirements](https://docs.brew.sh/Installation).
Apple's Command Line Tools installation requires confirmation in its system dialog: finish
that installation, then press Enter in the NetSpectre installer to continue.

The installer automates downloading tools, compiling, copying application resources and
setting up PATH. It cannot bypass administrator approvals, organizational restrictions,
unsupported operating systems or a required Windows restart. It reports failures instead of
claiming a partial installation succeeded. The Windows installer is implemented but still
needs verification on a clean Windows machine; do not treat cross-compilation as that test.

### Where things are installed

| | macOS | Windows |
| --- | --- | --- |
| Application and user data | `~/Library/Application Support/Modrion NetSpectre/app` | `%LOCALAPPDATA%\Modrion NetSpectre\app` |
| Terminal launcher | `~/.local/bin/netspro` and `Netspro` | `%LOCALAPPDATA%\Modrion NetSpectre\bin\netspro.cmd` |
| PATH setup | Appends a marked PATH line to Bash/Zsh profiles | Adds the launcher directory to the current user's PATH |

Launchers set the correct working directory automatically. The extracted source folder may
be moved or removed **after installation succeeds**. Personal `config.json`, cached catalogs,
Python environments and existing themes stay in the application directory.

### Update an existing installation

Close NetSpectre, extract a new source ZIP, and run the installer again. It rebuilds the
program and updates bundled catalog recipes. Existing configuration, downloaded models,
Python environments and edited themes are preserved. New theme files are added without
replacing themes you already have.

### Troubleshooting

| Problem | What to do |
| --- | --- |
| Windows opens the ZIP instead of running setup | Choose **Extract All**, then run `install.bat` from the extracted folder. |
| macOS refuses to open the script | Use **Open** from the file's context menu and follow macOS's approval UI. You can also run `bash /path/to/NetSpectre/install.command` in Terminal. |
| Apple tools are not ready | Finish the Command Line Tools dialog and rerun `install.command`. |
| Installer reports that a reboot is needed | Restart Windows and rerun `install.bat`. |
| `netspro` is not found | Open a new terminal. On Windows, sign out and back in if its parent process still has the old PATH. |
| You moved the source directory | Installed launchers remain valid. `start.command` is only for a local source-tree build. |
| Downloads or prerequisite installation fail | Read the first error, check connectivity/free space, and rerun setup. Completed system-tool installations are reused. |

To remove NetSpectre, remove its launcher and application folder, and remove the marked PATH
entry if desired. **Back up the application folder first: it contains your settings and data.**
The installers do not automatically uninstall shared developer tools or Homebrew.

## First start

NetSpectre opens an interactive login screen. Account access is currently a local development
mechanism, not production server authentication. Available commands depend on the account.

| Command inside NetSpectre | Purpose |
| --- | --- |
| `help` | Show available commands. |
| `credits` or `--credits` | Display project credits and attribution. |
| `devtools` | List developer tools. |
| `downloader` | Open the package catalog; requires a developer account. |
| `exit` | Close the application. |

The catalog remains developer-only even when the developer-mode display is disabled.
Use an interactive ANSI terminal at least **64 columns × 18 rows**. Mouse interaction depends
on terminal mouse-reporting support.

### Themes

Choose a theme through the `theme` property in the application's `config.json`.
Themes live in `themes/`. See [`themes/dark.json`](themes/dark.json) for a working example.
`main` controls normal output and the logo; `accent` controls highlighted text; `input`
controls typed input. Back up your configuration before editing it.

## Package catalog

The catalog includes Python, Ollama, an isolated Python environment, 27 Python libraries,
Homebrew itself, and **200 Homebrew formulae**, plus the live Ollama model library.

| Control | Action |
| --- | --- |
| Up/Down | Select an entry. |
| Enter | Open details; choose installation or model tags. |
| Left/Right or Page Up/Page Down | Change pages. |
| `/` or Search | Search names, descriptions, systems and versions. |
| `F` or Filters | Open combined filters and sorting. |
| Mouse hover/click/wheel | Highlight, select and navigate. |
| B / Escape | Return from details. |
| Q / Ctrl+C | Close the catalog. |

Before installing, the application shows the commands and missing dependencies.
Type **`INSTALL`** to proceed or press Enter to cancel. Command output and administrator
prompts remain visible. A failed command stops the plan; earlier completed installations
are not automatically rolled back.

### Filters and sorting

Filters combine with text search. Use Up/Down to choose a filter, Enter/Left/Right to change
it, or click its row. **R** resets filters; Escape or F returns to results.

- **Operating system:** Windows, macOS or Linux.
- **Standalone:** no separate application runtime required. This does not mean a portable
  executable with zero OS dependencies. Unknown Homebrew metadata stays **Unknown**.
- **Official source:** official registry/distribution, not manufacturer endorsement or a security audit.
- **Latest stable:** an unpinned stable/latest track in the available metadata; not a check
  of installed versions. Versioned Homebrew packages, the pinned Windows Python recipe and
  specific Ollama tags are excluded.
- **Source/category:** Homebrew, PyPI, Ollama, local recipes; managers, runtimes, libraries,
  formulae and models.
- **Sort:** catalog order, name, or Homebrew popularity.

### Homebrew Top 200

Rankings come from Homebrew's official [30-day direct-install statistics and metadata API](https://formulae.brew.sh/docs/api/).
The catalog selects the first 200 unique, enabled Homebrew/core formulae with stable releases
and published bottles. Unsupported entries and third-party taps are skipped. Counts represent
reported installations, **not unique users**. OS support is based on published bottle platforms.

[`catalog/homebrew.json`](catalog/homebrew.json) contains the initial snapshot and ranking dates.
Startup refreshes it asynchronously and saves an offline cache. Homebrew itself is installable
on macOS/Linux; its official installer may need system tools and administrator rights.
Homebrew handles each formula's dependencies. Native Windows is excluded; WSL is a Linux environment.

### Ollama models

Startup loads the [Ollama library](https://ollama.com/search), including subsequent pages.
Open a model to retrieve its current tags and quantization variants, then select an exact tag
to download it using `ollama pull`. Start the Ollama application or `ollama serve` beforehand.
Model weights may need several GB of storage. Hardware requirements and licenses vary by model.

Model lists and tags are cached for offline browsing. Ollama's public HTML is not a stable API;
upstream layout changes can require parser updates. **Hugging Face is not integrated yet.**

### Python packages

Libraries install into `.netspro-python/` inside the application directory, without modifying
system Python packages. Use `.netspro-python/bin/python` on macOS/Linux or
`.netspro-python/Scripts/python.exe` on Windows. Pip installs library dependencies.

Runtime installation supports Homebrew on macOS, WinGet on Windows, and apt on Debian/Ubuntu.
Other Linux distributions can preinstall Python, venv and Ollama. Installation recipes and
OS-specific requirements live in [`catalog/third_party.json`](catalog/third_party.json).

### Network and trust

The app refreshes catalog metadata at startup. HTTP requests have timeouts and bounded output;
network failures keep the available offline catalog. These are browsing requests, not automatic
package installations. Downloaded Homebrew names become fixed `brew install --formula` argument
vectors; remote command strings are never executed. Local installation recipes are executable
configuration and should be reviewed like code. Official Homebrew/Ollama bootstrap scripts are
downloaded over HTTPS to private temporary files before execution.

## Build from source

The JSON library is bundled; no dependency fetch is needed for compilation.
Requirements: **CMake 3.20+**, a **C++14 compiler**, and the platform's standard development tools.
Linux users also need `curl` for online catalog refreshes.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The default output is `Netspro` (`Netspro.exe` on Windows) in the project root. Run it from
that directory so it finds `themes/` and `catalog/`, or use `start.command` on macOS.
Installers use `NETSPRO_OUTPUT_DIRECTORY` to keep their build output inside `build/`.
MSVC builds link the C++ runtime statically.

For a temporary macOS installation without PATH edits or dependency bootstrapping:

```sh
bash scripts/install-macos.sh --prefix /tmp/netspro-test --no-path --skip-tools
```

## Development and tests

```sh
python3 tests/downloader_pty.py
python3 tests/installers_test.py  # macOS: temporary install; no PATH edits or tool bootstrap
c++ -std=c++14 -Isrc -Ithird_party tests/download_service_test.cpp src/download_service.cpp -o /tmp/netspro-service-test
/tmp/netspro-service-test
c++ -std=c++14 -Isrc -Ithird_party tests/catalog_sources_test.cpp src/catalog_sources.cpp src/download_service.cpp -o /tmp/netspro-catalog-test
/tmp/netspro-catalog-test
```

PTY tests run on macOS/Linux in isolated folders with mock package managers. Other tests cover
literal process arguments, dependencies, failure handling, Homebrew normalization and filters.
Clean-system installer tests and native Windows/Linux runtime coverage remain separate from
cross-builds. No automated test installs all 200 packages or downloads large model weights.

| Path | Responsibility |
| --- | --- |
| `src/main.cpp` | Application entry point and CLI. |
| `src/downloader.*` | Interactive catalog and terminal handling. |
| `src/download_service.*` | Processes, install plans and model transfers. |
| `src/catalog_sources.*`, `src/catalog_filters.h` | Registry metadata and filters. |
| `catalog/`, `themes/` | Bundled metadata, installation recipes and themes. |
| `install.command`, `install.bat`, `scripts/` | Installation entry points and platform setup. |
| `tests/` | Isolated regression tests. |
| `third_party/nlohmann/` | Bundled JSON headers and their MIT license. |

Build products, personal configuration, downloaded packages, generated caches and IDE state
are ignored by Git. Contribute source changes, tests, bundled catalogs and documentation.
The JARVIS and Welcome Setup components from the parent repository are not included here.

## License and credits

Copyright © 2026 **Hendrik Hanking**. Original development by Hendrik Hanking and the
**Modrion Intelligence Developer Team**.

Licensed under **GNU AGPL-3.0-only**, with additional attribution terms under Section 7(b).
Read [`LICENSE`](LICENSE) and [`ADDITIONAL_TERMS.md`](ADDITIONAL_TERMS.md).
Bundled nlohmann/json has its own [MIT license](third_party/nlohmann/LICENSE.MIT).
Third-party packages and models retain their respective licenses.

Project attribution is accessible inside NetSpectre with `credits` or `--credits`.
