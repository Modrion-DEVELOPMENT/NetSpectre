# Modrion NetSpectre

Modrion NetSpectre is also known as Nets or Netspro. This directory contains the standalone project, including its
application source code, CMake build file, and the JSON library required to build it. It does not depend on files from
the parent repository.

## Build

You need CMake 3.20 or later and an installed C++ compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The build places the `Netspro` executable (an alias for Modrion NetSpectre) in this directory.

## Run

Open a terminal in this directory and run:

```sh
./Netspro
```

On macOS, you can also open `start.command`.

In the CLI, enter `credits` to view the project credits and attribution. The `--credits` alias is also available.

## Download catalog (developer accounts)

Enter `downloader` (also listed by `devtools`). Access requires a developer account,
even when developer mode is off. The catalog includes 30 real entries: Python,
Ollama, a private Python environment, and 27 Python libraries. Installation recipes,
OS support and dependencies are in `catalog/third_party.json`.

At application startup, an asynchronous refresh reads the public
[Ollama model catalog](https://ollama.com/search), including its pagination.
Opening the downloader does not wait for the refresh. Successful refreshes update
the open catalog and `catalog/ollama-cache.json`; network failures retain the offline
catalog and show a status message. Refreshes have per-request timeouts and an overall
45-second pagination budget (an in-flight request can take another 15 seconds).
These public HTML pages are not a stable API; a layout change can require a parser update.

- Up/Down selects entries; Enter opens details. Left/Right or Page Up/Page Down changes pages.
- `/` or Search filters names, descriptions, OS and versions. Enter accepts; Escape cancels.
- Mouse hover highlights entries; click opens details; scrolling moves selection.
- For a package, Enter on **Install** shows the commands and dependency plan.
  Type `INSTALL` to execute; an empty answer cancels. Install output is shown directly,
  including package-manager progress and administrator prompts.
- For an Ollama model, **Browse tags** loads its current tags/quantization variants from
  its official `/library/<model>/tags` page. Select a variant to run `ollama pull <exact-tag>`.
  Tags are cached per model; bundled Llama 3.2 tags provide an initial offline fallback.
- B or Escape returns to the list; Q closes. Ctrl+C cancels a running installer on POSIX
  and is forwarded by the Windows console; commands report nonzero exits as failures.

Installations use argument vectors without shell command interpolation. Only local
catalog recipes define executable commands; remote pages supply validated model names/tags.
Review changes to this JSON as executable configuration. Dependencies are resolved in order,
checked before installation and verified afterward where a check is defined. Failed commands
stop the plan immediately; previously completed installations are not rolled back.

Python packages install into `.netspro-python/`, leaving system Python packages alone.
Use `.netspro-python/bin/python` (Windows: `.netspro-python/Scripts/python.exe`) to use them.
`latest` means the package manager selects the current compatible release; it is not a
claim about the installed version. Pip resolves each library's Python dependencies.

Runtime installation prerequisites:

- macOS: Homebrew must already be installed (`brew install python` / `brew install ollama`).
- Windows: WinGet must be available. A new runtime installation can require restarting
  the terminal to refresh PATH; failed verification reports this explicitly.
- Linux: Python installation supports Debian/Ubuntu via `sudo apt-get`. Ollama uses its
  [official installer](https://docs.ollama.com/linux), downloaded over HTTPS to a private
  temporary file before running `sh`. Other distributions can preinstall Python, venv and Ollama.
- Start the Ollama app or `ollama serve` before pulling a model. Model downloads can be large;
  authentication, disk capacity, model licensing and GPU compatibility depend on the chosen model.

Run from the project directory, in an interactive ANSI terminal of at least 64 x 18.
The browser restores normal terminal input while installers run and restores the original
screen and cursor when it closes. Mouse support depends on the terminal.

Verification (without installing system packages or downloading model weights):

```sh
python3 tests/downloader_pty.py
c++ -std=c++14 -Isrc -Ithird_party tests/download_service_test.cpp src/download_service.cpp -o /tmp/netspro-service-test
/tmp/netspro-service-test
```

Terminal tests use isolated directories and mock package managers. Service tests exercise
real subprocess creation, literal arguments, dependency ordering/cycles, cancellation and
failure propagation. Windows and Linux still require native runtime testing.

## Contents

- `src/main.cpp`: application entry point and implementation.
- `src/downloader.cpp` and `src/downloader.h`: interactive terminal catalog and live refresh.
- `src/download_service.cpp` and `.h`: HTTP fetches, tag parsing, dependency plans and process execution.
- `catalog/third_party.json`: platform-specific installation recipes and offline model defaults.
- `third_party/nlohmann/`: bundled nlohmann/json 3.12.0 headers and license; no download is needed to build the project.
- `themes/`: JSON theme files used by the terminal interface.
- `config.json`: created automatically on the first launch.

The JARVIS and Welcome Setup components from the parent repository are not included in this standalone project.

## License

Licensed under the GNU Affero General Public License v3.0 (AGPL-3.0).

Additional attribution terms apply under Section 7 (b).

See [ADDITIONAL_TERMS.md](ADDITIONAL_TERMS.md).
