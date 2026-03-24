# Cross-compilation toolchain: clang-cl targeting 32-bit Windows MSVC ABI
# Requires: llvm/clang installed, xwin run to populate XWIN_DIR

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR X86)

# ── Compilers ────────────────────────────────────────────────────────────────
# find_program respects cross-compilation root path restrictions even when
# FIND_ROOT_PATH_MODE_PROGRAM is set in the same file, so use execute_process
# (which always runs in the host shell) to locate binaries via PATH.
execute_process(
    COMMAND sh -c "which clang-cl 2>/dev/null || which clang-cl-19 2>/dev/null"
    OUTPUT_VARIABLE _CLANG_CL OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND sh -c "which lld-link 2>/dev/null || which lld-link-19 2>/dev/null"
    OUTPUT_VARIABLE _LLD_LINK OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
    COMMAND sh -c "which llvm-lib 2>/dev/null || which llvm-lib-19 2>/dev/null || which llvm-ar 2>/dev/null || which llvm-ar-19 2>/dev/null"
    OUTPUT_VARIABLE _LLVM_AR OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT _CLANG_CL)
    message(FATAL_ERROR "clang-cl not found in PATH. Install clang (Arch) or clang-19 (Ubuntu).")
endif()
if(NOT _LLD_LINK)
    message(FATAL_ERROR "lld-link not found in PATH. Install lld (Arch) or lld-19 (Ubuntu).")
endif()

set(CMAKE_C_COMPILER   "${_CLANG_CL}")
set(CMAKE_CXX_COMPILER "${_CLANG_CL}")
set(CMAKE_LINKER       "${_LLD_LINK}")
set(CMAKE_AR           "${_LLVM_AR}")
set(CMAKE_RANLIB true)

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
    "${XWIN_DIR}/sdk/include/winrt"
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

set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
