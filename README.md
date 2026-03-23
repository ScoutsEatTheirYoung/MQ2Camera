# MQ2Camera

A MacroQuest2/MQNext plugin for EverQuest that exposes camera position and orientation data to the MQ TLO (Top Level Object) system.

## Usage

Once loaded, the following TLO members are available in macros:

```
/echo ${Camera.X}
/echo ${Camera.Y}
/echo ${Camera.Z}
/echo ${Camera.Heading}
/echo ${Camera.Pitch}
/echo ${Camera.Roll}
/echo ${Camera}           -- prints "X=... Y=... Z=... H=..."
```

## Target

Built against the **MacroQuest LIVE branch** (x64, `x86_64-pc-windows-msvc` ABI). If you need RoF2 EMU support (32-bit), you would need the EMU branch of MacroQuest and a 32-bit toolchain — see the notes at the bottom.

---

## Compiling on Arch Linux

This is a Windows DLL compiled entirely on Linux via cross-compilation using the LLVM toolchain. No Windows machine or Wine required.

### Prerequisites

Install the following packages:

```bash
sudo pacman -S cmake ninja llvm clang lld
```

You also need **xwin** (downloads the Windows SDK and MSVC CRT headers/libs from Microsoft):

```bash
cargo install xwin
```

If you don't have Rust/Cargo:

```bash
sudo pacman -S rust
```

### Step 1 — Clone MacroQuest

The plugin compiles against the MacroQuest headers and pluginapi source. Clone it as a sibling of this repo:

```bash
cd ~/dev
git clone --recurse-submodules https://github.com/macroquest/macroquest
```

The directory structure must be:

```{txt}
~/dev/
  macroquest/
  mq2camera/       ← this repo
```

Or override the path at cmake configure time with `-DMQ_ROOT=/path/to/macroquest`.

### Step 2 — Fetch the Windows SDK via xwin

xwin downloads MSVC CRT headers, the Windows SDK headers, and their import libraries from Microsoft's servers. This is the legal way to get Windows headers on Linux.

```bash
xwin --accept-license splat --output ~/.xwin-cache/splat
```

This takes a few minutes and downloads ~1 GB. It only needs to be done once. The toolchain file (`toolchain-win32-clang.cmake`) auto-detects the versioned SDK path under `~/.xwin-cache/splat`.

### Step 3 — Download the DirectX SDK headers

MacroQuest's eqlib uses `d3dx9.h` / `d3dx9tex.h` from the legacy DirectX SDK. These are not in xwin. Download them from the Microsoft.DXSDK.D3DX NuGet package:

```bash
mkdir -p build/dxsdk
cd build/dxsdk
curl -L -o dxsdk.zip "https://www.nuget.org/api/v2/package/Microsoft.DXSDK.D3DX/9.29.952.8"
unzip dxsdk.zip -d dxsdk-pkg
mkdir -p headers/dxsdk-d3dx
cp dxsdk-pkg/build/native/include/* headers/dxsdk-d3dx/
cd ../..
```

### Step 4 — Set up third-party headers

spdlog, fmt, and glm are included as header-only libraries. Create a redirect directory containing only these — do **not** expose all of `/usr/include`, as Linux system headers conflict with the Windows SDK headers (type redefinition errors for `time_t`, `__float128`, etc.).

```bash
mkdir -p build/thirdparty_headers
ln -sf /usr/include/spdlog build/thirdparty_headers/spdlog
ln -sf /usr/include/fmt    build/thirdparty_headers/fmt
ln -sf /usr/include/glm    build/thirdparty_headers/glm
```

### Step 5 — Build the stub import libraries

The plugin links against `MQ2Main.dll` and `eqlib.dll`, which are only available at runtime on Windows. On Linux we create stub import libraries from `.def` files using `llvm-dlltool`.

```bash
mkdir -p build/stubs

# Empty stubs for libs referenced by #pragma comment inside MQ headers
llvm-lib /out:build/stubs/detours.lib
llvm-lib /out:build/stubs/imgui-64.lib
llvm-lib /out:build/stubs/pluginapi.lib

# Real stubs from .def files
llvm-dlltool -m i386:x86-64 --input-def MQ2Main.def --output-lib build/stubs/MQ2Main.lib
llvm-dlltool -m i386:x86-64 --input-def eqlib.def   --output-lib build/stubs/eqlib.lib
```

The `.def` files list every symbol from `MQ2Main.dll` and `eqlib.dll` that this plugin references. The C++ mangled names were extracted with `llvm-nm --undefined-only` on the compiled object files and must match the ABI of the actual DLLs exactly.

### Step 6 — Apply eqlib header patches

Two eqlib headers contain code that MSVC silently accepts but clang rejects. Patched copies live in `build/patches/` and shadow the originals via CMake's include path ordering.

**`build/patches/eqlib/game/CXStr.h`** — Fix an incorrect `std::string_view::compare` call:

```cpp
// Original (MSVC tolerates, clang rejects):
compare(pos1, count1, 2, pos2, count2)
// Patched:
compare(pos1, count1, std::string_view{t}, pos2, count2)
```

**`build/patches/eqlib/game/SoeUtil.h`** — Fix ambiguous conditional with `std::atomic_int32_t`:

```cpp
// Original:
m_rep->m_strongRefs : 0
// Patched:
m_rep->m_strongRefs.load() : 0
```

If you regenerate these patches, copy the originals from `macroquest/src/eqlib/include/eqlib/game/` and apply the minimal changes above.

### Step 7 — Configure and build

```bash
cmake -B build/cmake -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=toolchain-win32-clang.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --target MQ2Camera
```

The output DLL will be at `build/cmake/bin/MQ2Camera.dll`.

---

## Roadblocks and How They Were Solved

These are the non-obvious problems you will hit if you attempt this from scratch.

**1. The main MacroQuest branch is LIVE (x64), not RoF2 (x86)**
The MacroQuest `main` branch includes `BuildType.h` which defines `LIVE` and enforces x64 via `#error x64 Configuration is required`. The toolchain must target `x86_64-pc-windows-msvc`, not `i686-pc-windows-msvc`. If you need RoF2 EMU (32-bit), you need the EMU branch and need to change the target triple and machine in the toolchain file.

**2. xwin uses a versioned SDK path**
xwin places headers under `sdk/include/10.0.26100/` (or whatever version it downloaded), not directly under `sdk/include/`. The toolchain file uses a CMake `file(GLOB ...)` to find this dynamically rather than hardcoding the version number.

**3. xwin only includes the Release CRT**
`msvcrtd.lib` (debug CRT) is not redistributable and is not in xwin. CMake defaults to linking the debug CRT in Debug builds. Fix: set `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"` in the toolchain file to force `/MD` for all configurations.

**4. Linux system headers conflict with Windows SDK headers**
If you naively add `/usr/include` to the include path for spdlog/fmt, Linux's `time_t`, `__float128`, and other type definitions clash with the Windows SDK. Fix: create `build/thirdparty_headers/` containing only symlinks to spdlog, fmt, and glm — nothing else.

**5. `DEPRECATE()` macros cause hard errors in clang**
MQ uses a `DEPRECATE(x)` macro that expands to `[[deprecated(x)]]`. Clang rejects `[[deprecated]]` in certain positions that MSVC allows (e.g. on typedefs and in certain template contexts). The official MQ fix is to define `COMMENT_UPDATER=1`, which makes `DEPRECATE(x)` expand to nothing.

**6. DirectX SDK headers not in xwin**
`d3dx9.h` and `d3dx9tex.h` are from the legacy DirectX SDK, which Microsoft stopped distributing in the standard Windows SDK. They are available via the `Microsoft.DXSDK.D3DX` NuGet package — same source as vcpkg's `directx-dxsdk-d3dx` port.

**7. Stub import library symbols must match exactly**
The C++ mangled names for `MQ2Type` constructor/destructor/methods and `GetMainInterface` must be the exact same mangled names as in the real DLLs. The way to get them right is to compile the object files first, then run `llvm-nm --undefined-only` on them to see exactly which symbols the linker needs, then add those to the `.def` files.

**8. Two eqlib headers have clang-incompatible code**
`CXStr.h` calls `std::string_view::compare` with an integer `2` where a `string_view` is expected (a typo that MSVC's overload resolution papers over). `SoeUtil.h` uses an `std::atomic_int32_t` directly in a ternary expression where an `int` is expected. Both require minimal one-line patches.

**9. `imgui-64.lib` and `pluginapi.lib` referenced via `#pragma comment`**
`MQ2Main.h` and `Plugin.h` include `#pragma comment(lib, ...)` directives for these libraries. The linker expects them to exist even though `imgui` lives inside `MQ2Main.dll` at runtime and `pluginapi.cpp` is compiled directly into the plugin. Fix: create empty stub `.lib` files using `llvm-lib`.

---

## File Overview

| File | Purpose |
|------|---------|
| `MQ2Camera.cpp` | Plugin source — defines `MQ2CameraType`, registers `${Camera}` TLO |
| `CMakeLists.txt` | Standalone cross-compilation build |
| `toolchain-win32-clang.cmake` | CMake toolchain: clang-cl + lld-link targeting x64 Windows MSVC ABI |
| `MQ2Main.def` | Export definitions for MQ2Main.dll stub import lib |
| `eqlib.def` | Export definitions for eqlib.dll stub import lib |
| `build/patches/` | Patched eqlib headers that shadow the originals for clang compatibility |
| `.vscode/` | IntelliSense and build task configuration |

---

## RoF2 EMU Note

If you want to target the RoF2 EMU client (32-bit), you need:
- The MacroQuest EMU branch (not `main`)
- Change `MQ_TARGET_TRIPLE` in the toolchain to `i686-pc-windows-msvc`
- Change `/machine:x64` to `/machine:x86` throughout the toolchain
- Change `llvm-dlltool -m i386:x86-64` to `llvm-dlltool -m i386` when building stub libs
- Use xwin with `--arch x86` (or ensure x86 libs are present in your xwin cache)

---

## License

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License** as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
