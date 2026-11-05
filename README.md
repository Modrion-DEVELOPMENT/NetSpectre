# Modrion Netspro

Dieses Verzeichnis ist ein eigenständiges Netspro-Projekt. Es enthält den Anwendungscode, CMake-Datei und die JSON-Bibliothek, die zum Bauen gebraucht wird. Es benötigt keine Dateien aus dem übergeordneten Netspro-Projekt.

## Bauen

Voraussetzung ist CMake 3.20 oder neuer sowie ein installierter C++-Compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Der Build legt das Programm `Netspro` direkt in diesem Verzeichnis ab.

## Starten

Im Terminal in dieses Verzeichnis wechseln und starten:

```sh
./Netspro
```

Auf macOS kann alternativ `start.command` geöffnet werden.

## Inhalt

- `src/main.cpp`: Hauptprogramm.
- `third_party/nlohmann/`: gebündelte nlohmann/json-3.12.0-Header und Lizenz; beim Bauen ist kein Download nötig.
- `config.json`: wird beim ersten Programmstart automatisch angelegt.

Die JARVIS- und Welcome-Setup-Komponenten aus dem Hauptprojekt sind nicht Teil dieses eigenständigen Projekts.
