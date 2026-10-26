# War3 Map Exit Fix

Runtime fixes for Warcraft III Classic crashes when leaving custom maps, plus menu cursor restoration after returning from a map.

## Download and install

Download the ZIP from [Releases](https://github.com/EVSkyFall/War3-Map-Exit-Fix/releases/latest).

1. Close Warcraft III.
2. Extract `War3MapExitFix.mix` and `War3PostGameFix.mix` into the Warcraft III folder containing `Game.dll`.
3. Start the game normally.

The game's MIX loader loads the plugins automatically. No launcher changes are needed. To uninstall, close the game and remove the two MIX files. When upgrading, replace the existing copies; keep backups outside the game folder.

## What it fixes

| Plugin | Behavior |
| --- | --- |
| `War3MapExitFix.mix` | Handles a missing UI frame registration during map teardown, allowing the remaining frame cleanup to finish. |
| `War3PostGameFix.mix` | Retires the previous map's script callback and queued UI events after gameplay ends, while preserving the game's normal UI tick and the next map's registrations. |
| Menu cursor component, included in `War3PostGameFix.mix` | Restores the native menu cursor after a map uses its own cursor model. In-game custom cursors and other skin entries retain their original behavior. |

The plugins modify functions in memory. They do not rewrite `Game.dll`, `dzclient_api.dll`, or map files. They locate functions from the loaded code rather than requiring a particular file hash. Incomplete setup is retried, and cursor setup does not block the crash fixes.

## Runtime

- Windows, 32-bit Warcraft III Classic.
- Developed and tested against **1.28.5.7680** with JN/YDWE.
- `War3MapExitFix.mix` works on the game's frame cleanup path independently of dzclient.
- `War3PostGameFix.mix` uses the UI dispatcher in **`dzclient_api.dll`**, supplied by JN/YDWE. Without that module, it waits for the module to load.

The current code patterns describe the analyzed Classic layouts; x64/Reforged support has not been implemented.

Installation messages are written beside the plugins:

- `War3MapExitFix.log`: `INSTALLED missing-frame-registry-entry repair`
- `War3PostGameFix.log`: `INSTALLED post-game callback lifecycle repair` and `INSTALLED native menu cursor restoration`

`CURSOR native menu model loaded and bound` records a completed native cursor rebind. Logging failure does not prevent the repairs from running.

## Validation

The two map-exit crash fixes have been confirmed in an actual play session. Native offline checks cover missing frame entries, normal cleanup, menu/next-map callback lifetime, retry behavior, and menu cursor selection and binding.

Menu cursor visibility has also been confirmed in an actual play session. Automated cursor tests use controlled model-loading and sprite-assignment fixtures; the visual confirmation comes from play feedback, not those fixtures. See [validation details](docs/validation.md).

## Build

Install Python 3.9+ and [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw/releases). The compiler target is Windows x86; the build host may be Windows or Linux.

```sh
python tools/build.py --toolchain /path/to/llvm-mingw
python tools/package.py --version 1.0.0
```

Outputs:

```text
build/map-exit/War3MapExitFix.mix
build/post-game/War3PostGameFix.mix
dist/War3-Map-Exit-Fix-v1.0.0.zip
```

No game files are needed to build. Optional native replay tests use privately supplied local DLLs; see [building and testing](docs/building.md).

## Credits and license

MIT licensed. This project grew out of work alongside [WC3FontAtlasFix](https://github.com/s09289728096/WC3FontAtlasFix), whose notice is retained. The font-atlas plugin is a separate project and is not included in this release.

[MinHook](https://github.com/TsudaKageyu/minhook) and its HDE components are vendored with their original licenses. See [third-party notices](THIRD_PARTY_NOTICES.md).
