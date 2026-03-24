# Building MQ2Camera on Arch Linux

This is a Windows DLL compiled entirely on Linux via cross-compilation using the LLVM toolchain. No Windows machine or Wine required.

## Prerequisites

```bash
sudo pacman -S cmake ninja llvm clang lld
cargo install xwin   # needs rust: sudo pacman -S rust
```

## Step 1 — Clone MacroQuest (EMU branch)

```bash
cd ~/dev
git clone --depth=1 --branch emu \
  https://github.com/macroquest/macroquest
git -C macroquest submodule update --init src/eqlib
```

The directory structure must be:

```
~/dev/
  macroquest/
  mq2camera/       ← this repo
```

Or override at configure time with `-DMQ_ROOT=/path/to/macroquest`.

## Step 2 — Fetch the Windows SDK via xwin

```bash
xwin --accept-license --arch x86 splat --output ~/.xwin-cache/splat
```

Downloads MSVC CRT + Windows SDK headers and x86 libs (~1 GB, one-time).

## Step 3 — Download DirectX SDK headers

```bash
mkdir -p build/dxsdk
curl -L -o build/dxsdk/dxsdk.zip \
  "https://www.nuget.org/api/v2/package/Microsoft.DXSDK.D3DX/9.29.952.8"
unzip build/dxsdk/dxsdk.zip -d build/dxsdk/pkg
mkdir -p build/dxsdk/headers/dxsdk-d3dx
cp build/dxsdk/pkg/build/native/include/* build/dxsdk/headers/dxsdk-d3dx/
```

## Step 4 — Download WIL headers

```bash
mkdir -p build/wil
curl -L -o build/wil/wil.zip \
  "https://www.nuget.org/api/v2/package/Microsoft.Windows.ImplementationLibrary"
unzip build/wil/wil.zip -d build/wil/pkg
```

## Step 5 — Set up third-party header symlinks

```bash
mkdir -p build/thirdparty_headers
ln -sf /usr/include/spdlog  build/thirdparty_headers/spdlog
ln -sf /usr/include/fmt     build/thirdparty_headers/fmt
ln -sf /usr/include/glm     build/thirdparty_headers/glm
ln -sf $PWD/build/wil/pkg/include/wil build/thirdparty_headers/wil
```

## Step 6 — Build stub import libraries

```bash
mkdir -p build/stubs
llvm-dlltool -m i386 --input-def MQ2Main.def --output-lib build/stubs/MQ2Main.lib
llvm-dlltool -m i386 --input-def eqlib.def   --output-lib build/stubs/eqlib.lib
for lib in detours imgui imgui-64 pluginapi; do
  llvm-ar rcs build/stubs/${lib}.lib
done
```

## Step 7 — Apply eqlib header patches

Two eqlib headers have code clang rejects but MSVC silently accepts. Patched copies in `build/patches/` shadow the originals via CMake's include order.

```bash
mkdir -p build/patches/eqlib/game

cp ../macroquest/src/eqlib/include/eqlib/game/CXStr.h build/patches/eqlib/game/CXStr.h
sed -i 's/compare(pos1, count1, 2,/compare(pos1, count1, std::string_view{t},/' \
  build/patches/eqlib/game/CXStr.h

cp ../macroquest/src/eqlib/include/eqlib/game/SoeUtil.h build/patches/eqlib/game/SoeUtil.h
sed -i 's/m_rep->m_strongRefs : 0/m_rep->m_strongRefs.load() : 0/' \
  build/patches/eqlib/game/SoeUtil.h
```

## Step 8 — Configure and build

```bash
cmake -B build/cmake -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=toolchain-win32-clang.cmake \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build/cmake --target MQ2Camera
```

Output: `build/cmake/bin/MQ2Camera.dll`

---

## Roadblocks and How They Were Solved

**1. MacroQuest main branch is LIVE (x64), not RoF2 (x86)**
The `master` branch defines `LIVE` and requires x64. Use the `emu` branch (`--branch emu`), which defines `EMULATOR` and `EXPANSION_LEVEL_ROF` for the 32-bit RoF2 client.

**2. xwin uses a versioned SDK path**
xwin puts headers under `sdk/include/10.0.26100/` not `sdk/include/`. The toolchain uses `file(GLOB ...)` to find the version directory dynamically.

**3. xwin only includes the Release CRT**
`msvcrtd.lib` is not in xwin. Toolchain sets `CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL"` to force `/MD` for all configs.

**4. Linux system headers conflict with Windows SDK**
Adding `/usr/include` exposes Linux types (`time_t`, `__float128`) that clash with the Windows SDK. Fix: `build/thirdparty_headers/` contains only symlinks to spdlog, fmt, glm, and wil — nothing else.

**5. `DEPRECATE()` macros cause hard errors in clang**
MQ's `DEPRECATE(x)` expands to `[[deprecated(x)]]` in positions clang rejects. Define `COMMENT_UPDATER=1` — the official MQ mechanism to make it expand to nothing.

**6. DirectX SDK headers not in xwin**
`d3dx9.h` is from the legacy DirectX SDK, not in the standard Windows SDK. Available via NuGet: `Microsoft.DXSDK.D3DX`.

**7. WIL headers not in xwin**
eqlib includes `<wil/com.h>` (Windows Implementation Library). Available via NuGet: `Microsoft.Windows.ImplementationLibrary`. Also needs `sdk/include/winrt` in the system include path for `WeakReference.h`.

**8. Stub library mangled names must be exact**
C++ mangled names for `MQ2Type` methods differ between x86 and x64. Compile the objects first, then run `llvm-nm --undefined-only` on them to extract the exact symbols needed, then add to `.def` files.

**9. Two eqlib headers have clang-incompatible code**
`CXStr.h` passes `2` where `std::string_view` is expected. `SoeUtil.h` uses `std::atomic_int32_t` directly in a ternary. Both fixed with one-line sed patches.

**10. `imgui.lib` / `pluginapi.lib` referenced via `#pragma comment`**
MQ headers pull in libs via `#pragma comment(lib, ...)`. Create empty stubs with `llvm-ar rcs`.

---

## File Overview

| File | Purpose |
|------|---------|
| `MQ2Camera.cpp` | Plugin source |
| `CMakeLists.txt` | Standalone cross-compilation build |
| `toolchain-win32-clang.cmake` | clang-cl + lld-link targeting i686 Windows MSVC ABI |
| `MQ2Main.def` | Export definitions for MQ2Main.dll stub |
| `eqlib.def` | Export definitions for eqlib.dll stub |
| `build/patches/` | Patched eqlib headers for clang compatibility |
