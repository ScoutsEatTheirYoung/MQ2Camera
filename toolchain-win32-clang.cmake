# Cross-compilation toolchain: clang-cl targeting 32-bit Windows MSVC ABI
# Requires: llvm/clang installed, xwin run to populate XWIN_DIR

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR X86)

# ── Compilers ────────────────────────────────────────────────────────────────
set(CMAKE_C_COMPILER   clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_LINKER       lld-link)
set(CMAKE_AR           llvm-lib)
set(CMAKE_RANLIB       true)

# ── Target triple ─────────────────────────────────────────────────────────────
# RoF2 EMU client is 32-bit. LIVE branch would use x86_64-pc-windows-msvc + /machine:x64.
set(MQ_TARGET_TRIPLE "i686-pc-windows-msvc")
set(CMAKE_C_FLAGS_INIT   "--target=${MQ_TARGET_TRIPLE}")
set(CMAKE_CXX_FLAGS_INIT "--target=${MQ_TARGET_TRIPLE}")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "/machine:x86")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "/machine:x86")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "/machine:x86")

# ── xwin SDK path ─────────────────────────────────────────────────────────────
# Set XWIN_DIR environment variable before running cmake, or override here.
# Default: ~/.xwin-cache/splat
if(DEFINED ENV{XWIN_DIR})
    set(XWIN_DIR "$ENV{XWIN_DIR}")
else()
    set(XWIN_DIR "$ENV{HOME}/.xwin-cache/splat")
endif()

# ── Windows SDK / CRT headers via xwin ────────────────────────────────────────
# xwin splats headers under sdk/include/<version>/ — find the version dir
file(GLOB _XWIN_SDK_VER_DIRS "${XWIN_DIR}/sdk/include/[0-9]*")
list(GET _XWIN_SDK_VER_DIRS 0 _XWIN_SDK_VER_DIR)
if(NOT _XWIN_SDK_VER_DIR)
    message(FATAL_ERROR "xwin SDK not found at ${XWIN_DIR}. Run: xwin --accept-license --arch x86 splat --output ${XWIN_DIR}")
endif()

include_directories(SYSTEM
    "${XWIN_DIR}/crt/include"
    "${_XWIN_SDK_VER_DIR}/ucrt"
    "${_XWIN_SDK_VER_DIR}/um"
    "${_XWIN_SDK_VER_DIR}/shared"
)

# ── Windows SDK / CRT libs via xwin ───────────────────────────────────────────
link_directories(
    "${XWIN_DIR}/crt/lib/x86"
    "${XWIN_DIR}/sdk/lib/um/x86"
    "${XWIN_DIR}/sdk/lib/ucrt/x86"
)

# ── Don't look for Linux system libraries ─────────────────────────────────────
# ── Force release CRT (xwin doesn't include msvcrtd.lib) ─────────────────────
# This sets /MD (MultiThreadedDLL) for all configs, avoiding msvcrtd.lib
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
