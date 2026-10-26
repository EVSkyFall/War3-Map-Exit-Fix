# Building and testing

## Toolchain

Python's standard library drives the build. LLVM-MinGW provides Windows headers, libraries and x86 compiler tools. No Visual Studio SDK or proprietary game files are needed to compile.

The published release uses LLVM-MinGW 20250613 UCRT, Clang 20.1.7. Runtime libraries are linked statically; Windows 10 and newer include UCRT. The build accepts another installed LLVM-MinGW toolchain through `--toolchain` or PATH.

```sh
python tools/build.py --toolchain /path/to/llvm-mingw --tests
```

Use `--component map-exit` or `--component post-game` to build one plugin. Omit `--tests` when only the plugins are needed. Output manifests record the compiler and artifact hashes.

`guard.c` is compiled with Clang's MSVC target for native x86 SEH; all other sources use the MinGW target. Each plugin contains its own MinHook state. No runtime download is required.

## Native replay

These are bounded developer diagnostics, not game launchers. They map local DLLs without initialization and execute selected native functions with synthetic objects and controlled callbacks.

```sh
python tools/run_native.py --game /private/Game.dll --dz /private/dzclient_api.dll
```

The fixtures use the exact layouts analyzed during development. The runner checks their diagnostic fingerprints before executing fixed-address test code. This check is confined to the test runner; production plugins do not use a hash allowlist.

- Game.dll 1.28.5.7680: `05285a28e5c55a0be0c2948ee7e3b41426a1b6d74359b7d3ea57ea322f690420`
- dzclient_api.dll: `cb0f0c9b145d7a1c9e366733fc138af260f944fba2c95adf6664840e151457c0`

Supply your own DLLs and keep them outside tracked files. Never commit maps, dumps, game binaries or private logs. The diagnostics require Windows to execute; a Linux host can compile the plugins and test programs.

## Package

```sh
python tools/package.py --version 1.0.0
```

The ZIP contains both MIX files, installation documentation, license notices and per-file checksums. The adjacent checksum file covers the ZIP itself. Packaging checks the compiler manifests against the artifacts it is about to distribute.

CI builds and packages on Windows without game files. It compiles the native diagnostics but does not run the proprietary-input replays.
