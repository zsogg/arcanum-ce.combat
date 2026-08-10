# Cross-compile a Windows x64 binary from Linux with mingw-w64.
#
# Why this exists: CE's own `windows-*` presets use the Visual Studio 2022
# generator and carry `"condition": hostSystemName == Windows`, so they cannot
# run from a Linux host at all. This is the alternative path - same sources, a
# different compiler - for producing a playable .exe without a Windows machine.
#
# It is viable because `src/` and `first_party/tig/` contain zero MSVC-specific
# constructs: no `__declspec`, no `#pragma warning`, no `_MSC_VER` branches.
# The port is plain C17.
#
# Needs the toolchain installed first (Debian/Ubuntu):
#
#     sudo apt install mingw-w64
#
# Then:
#
#     cmake --preset windows-x64-mingw
#     cmake --build --preset windows-x64-mingw
#
# The result lands in out/build/windows-x64-mingw/arcanum-ce.exe and needs the
# mingw runtime DLLs beside it unless statically linked - see the flags below,
# which link them in so the .exe is self-contained and can simply be copied.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

# Look for programs on the host, but headers and libraries only in the target
# sysroot - otherwise CMake happily finds the host's Linux libraries.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Static-link libgcc and the winpthreads runtime so the produced .exe is
# self-contained. Without this it needs libgcc_s_seh-1.dll and
# libwinpthread-1.dll copied alongside, which is a support burden for something
# that only exists to be handed to a playtester.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static -static-libstdc++")
