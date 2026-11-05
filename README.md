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

## Contents

- `src/main.cpp`: application entry point and implementation.
- `third_party/nlohmann/`: bundled nlohmann/json 3.12.0 headers and license; no download is needed to build the project.
- `themes/`: JSON theme files used by the terminal interface.
- `config.json`: created automatically on the first launch.

The JARVIS and Welcome Setup components from the parent repository are not included in this standalone project.

## License

Licensed under the GNU Affero General Public License v3.0 (AGPL-3.0).

Additional attribution terms apply under Section 7 (b).

See [ADDITIONAL_TERMS.md](ADDITIONAL_TERMS.md).
