#!/usr/bin/env bash
# build.sh — Set up and cross-compile MQ2Camera.dll on Arch Linux
# Uses: clang-cl + lld-link + xwin (Windows SDK) + stub import libs

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MQ_ROOT="$(cd "${SCRIPT_DIR}/../macroquest" && pwd)"
XWIN_DIR="${HOME}/.xwin-cache/splat"
STUBS_DIR="${SCRIPT_DIR}/build/stubs"
BUILD_DIR="${SCRIPT_DIR}/build/cmake"

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[+]${NC} $*"; }
warn()  { echo -e "${YELLOW}[!]${NC} $*"; }
error() { echo -e "${RED}[!]${NC} $*"; exit 1; }

# ── 1. Check prerequisites ────────────────────────────────────────────────────
info "Checking prerequisites..."
command -v cmake    >/dev/null 2>&1 || error "cmake not found. Run: sudo pacman -S cmake"
command -v ninja    >/dev/null 2>&1 || error "ninja not found. Run: sudo pacman -S ninja"
command -v clang-cl >/dev/null 2>&1 || error "clang-cl not found. Run: sudo pacman -S clang"
command -v lld-link >/dev/null 2>&1 || error "lld-link not found. Run: sudo pacman -S lld"
command -v llvm-dlltool >/dev/null 2>&1 || error "llvm-dlltool not found. Run: sudo pacman -S llvm"
command -v llvm-ar  >/dev/null 2>&1 || error "llvm-ar not found. Run: sudo pacman -S llvm"

# ── 2. Download Windows SDK via xwin ─────────────────────────────────────────
if [[ ! -d "${XWIN_DIR}/sdk" ]]; then
    info "Downloading Windows SDK + MSVC CRT via xwin..."
    command -v xwin >/dev/null 2>&1 || error "xwin not found. Run: cargo install xwin"
    xwin --accept-license splat --output "${XWIN_DIR}" \
        --arch x86 \
        --variant desktop
    info "Windows SDK downloaded to ${XWIN_DIR}"
else
    info "Windows SDK already at ${XWIN_DIR}"
fi

# ── 3. Create case-sensitivity symlink (blech/ → Blech/) ─────────────────────
if [[ ! -e "${MQ_ROOT}/contrib/blech" ]]; then
    info "Creating contrib/blech symlink (case fix for Linux)..."
    ln -sf "${MQ_ROOT}/contrib/Blech" "${MQ_ROOT}/contrib/blech"
fi

# ── 4. Create stub import libraries ───────────────────────────────────────────
info "Creating stub import libraries..."
mkdir -p "${STUBS_DIR}"

# MQ2Main.lib — import stub for MQ2Main.dll
if [[ ! -f "${STUBS_DIR}/MQ2Main.lib" ]]; then
    info "  Building MQ2Main.lib stub..."
    llvm-dlltool \
        --machine i386 \
        --input-def "${SCRIPT_DIR}/MQ2Main.def" \
        --output-lib "${STUBS_DIR}/MQ2Main.lib" \
        --kill-at
fi

# eqlib.lib — import stub for eqlib.dll
if [[ ! -f "${STUBS_DIR}/eqlib.lib" ]]; then
    info "  Building eqlib.lib stub..."
    llvm-dlltool \
        --machine i386 \
        --input-def "${SCRIPT_DIR}/eqlib.def" \
        --output-lib "${STUBS_DIR}/eqlib.lib" \
        --kill-at
fi

# detours.lib — empty stub (pluginapi.cpp has #pragma comment(lib,"detours.lib")
# but our plugin doesn't use any Detours functions directly)
if [[ ! -f "${STUBS_DIR}/detours.lib" ]]; then
    info "  Building empty detours.lib stub..."
    # Create a minimal COFF object file for the 32-bit Windows target
    TMP_C=$(mktemp /tmp/detours_stub_XXXXXX.c)
    echo "// empty detours stub" > "${TMP_C}"
    TMP_OBJ="${TMP_C%.c}.obj"
    clang-cl --target=i686-pc-windows-msvc -c "${TMP_C}" -o "${TMP_OBJ}" 2>/dev/null || true
    if [[ -f "${TMP_OBJ}" ]]; then
        llvm-ar rcs "${STUBS_DIR}/detours.lib" "${TMP_OBJ}"
    else
        # Fallback: create an empty archive (lld-link may warn but not error)
        llvm-ar rcs "${STUBS_DIR}/detours.lib"
    fi
    rm -f "${TMP_C}" "${TMP_OBJ}"
fi

# ── 5. Check for spdlog/fmt/glm system headers ───────────────────────────────
for pkg_header in "/usr/include/spdlog/spdlog.h" "/usr/include/fmt/format.h" "/usr/include/glm/glm.hpp"; do
    if [[ ! -f "${pkg_header}" ]]; then
        warn "Missing: ${pkg_header}"
        warn "Install with: sudo pacman -S spdlog fmt glm"
    fi
done

# ── 6. CMake configure ────────────────────────────────────────────────────────
info "Configuring CMake build..."
mkdir -p "${BUILD_DIR}"
cmake -S "${SCRIPT_DIR}" \
      -B "${BUILD_DIR}" \
      -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="${SCRIPT_DIR}/toolchain-win32-clang.cmake" \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DMQ_ROOT="${MQ_ROOT}" \
      -DSTUB_LIBS_DIR="${STUBS_DIR}"

# ── 7. Build ──────────────────────────────────────────────────────────────────
info "Building MQ2Camera.dll..."
cmake --build "${BUILD_DIR}" --target MQ2Camera

DLL="${BUILD_DIR}/bin/MQ2Camera.dll"
if [[ -f "${DLL}" ]]; then
    info "SUCCESS: ${DLL}"
    info "Copy MQ2Camera.dll to your MacroQuest plugins/ directory"
    info "Then in game: /plugin MQ2Camera"
    info "Test: /echo \${Camera.X}"
else
    error "Build failed — DLL not produced"
fi
