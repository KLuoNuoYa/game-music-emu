# AGENTS.md

## Purpose

This repository is `libgme` plus a Windows-only `stdcall` wrapper DLL for
Inno Setup. The wrapper lives in [inno/gme_inno.cpp](./inno/gme_inno.cpp)
and exposes a small API that Pascal Script can call directly.

The primary deliverable for this repo is currently the static-runtime Win32 DLL:

- [build-win32-static/inno/Release/gme_inno.dll](./build-win32-static/inno/Release/gme_inno.dll)

Use that build for installer integration unless the user explicitly asks for
another variant.

## Current architecture

- Upstream library sources remain under [gme/](./gme).
- The Inno Setup wrapper is under [inno/](./inno).
- Root [CMakeLists.txt](./CMakeLists.txt) enables the wrapper with `GME_BUILD_INNO_DLL`.
- MSVC is configured for static runtime with `CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreaded...`.

## Important behavior

- `GMEInnoOpenFileW(path, sample_rate)` opens a file and selects the default track.
- `GMEInnoOpenFileTrackW(path, sample_rate, track_index)` opens a file and selects a specific track.
- `GMEInnoStartTrack(track_index)` switches tracks after open.
- Track numbering is zero-based.
- If `GMEInnoOpenFileTrackW(..., N)` is used, do not immediately call `GMEInnoStartTrack(0)` unless you intentionally want to reset playback to the first track.
- The wrapper is single-instance. It is designed for installer background music, not multi-stream playback.

## Exported API

Declared in [inno/gme_inno.h](./inno/gme_inno.h):

- `GMEInnoOpenFileW`
- `GMEInnoOpenFileTrackW`
- `GMEInnoClose`
- `GMEInnoStartTrack`
- `GMEInnoPlay`
- `GMEInnoPause`
- `GMEInnoStop`
- `GMEInnoIsPlaying`
- `GMEInnoSetLoop`
- `GMEInnoSetVolume`
- `GMEInnoGetTrackCount`
- `GMEInnoGetCurrentTrack`
- `GMEInnoGetLastErrorW`
- `GMEInnoGetLastErrorLength`

The `.def` generation in [inno/CMakeLists.txt](./inno/CMakeLists.txt)
and [inno/gme_inno.def.in](./inno/gme_inno.def.in) is important.
It keeps 32-bit `stdcall` exports predictable for Inno Setup.

## Build guidance

Prefer `Win32`, not `x64`, for Inno Setup compatibility.

### Recommended build

```powershell
& '/path/to/cmake.exe' -S . -B build-win32-static -G 'Visual Studio 17 2022' -A Win32 -DGME_BUILD_SHARED=OFF -DGME_BUILD_INNO_DLL=ON -DGME_BUILD_EXAMPLES=OFF -DGME_BUILD_TESTING=OFF
& '/path/to/cmake.exe' --build build-win32-static --config Release --target gme_inno
```

### Visual Studio discovery

If tool paths need rediscovery, use:

```powershell
& '/path/to/vswhere.exe' -latest -products * -requires Microsoft.Component.MSBuild -format json
```

## Runtime dependency rule

The preferred DLL must not depend on VC++ Redistributable components such as:

- `MSVCP140.dll`
- `VCRUNTIME140.dll`

The static-runtime build was verified with `dumpbin /dependents` and should only
depend on system DLLs such as `WINMM.dll` and `KERNEL32.dll`.

If a future change reintroduces VC++ runtime dependencies, treat that as a regression
unless the user explicitly asks for a dynamic-runtime build.

## Verification workflow

After modifying the wrapper, verify these:

1. Rebuild `build-win32-static`.
2. Check exports:

```powershell
& '/path/to/dumpbin.exe' /exports build-win32-static\inno\Release\gme_inno.dll
```

3. Check dependencies:

```powershell
& '/path/to/dumpbin.exe' /dependents build-win32-static\inno\Release\gme_inno.dll
```

4. Run the 32-bit smoke test:

```powershell
& '/path/to/32-bit/powershell.exe' -NoProfile -ExecutionPolicy Bypass -File './inno/smoke_test_static.ps1'
```

Expected output:

- `STATIC_OPEN_OK tracks=1 play=1`

## Inno Setup integration notes

- Example script: [inno/example.iss](./inno/example.iss)
- Use `files:` external loading syntax in Inno Setup.
- Ensure the installer references the static build DLL, not an older dynamic build.
- If a user reports `Could not call proc.`, first suspect:
  - stale DLL being packaged
  - wrong export declaration in `.iss`
  - architecture mismatch

## Files worth checking first

- [CMakeLists.txt](./CMakeLists.txt)
- [inno/CMakeLists.txt](./inno/CMakeLists.txt)
- [inno/gme_inno.h](./inno/gme_inno.h)
- [inno/gme_inno.cpp](./inno/gme_inno.cpp)
- [inno/README.md](./inno/README.md)
- [inno/example.iss](./inno/example.iss)
- [inno/smoke_test.ps1](./inno/smoke_test.ps1)
- [inno/smoke_test_static.ps1](./inno/smoke_test_static.ps1)

## Maintenance guidance

- Do not remove the `.def`-based export aliasing unless you also update and verify Inno integration.
- Keep wrapper changes minimal and installer-focused.
- Preserve upstream `libgme` behavior unless there is a clear wrapper-specific reason.
- When debugging track selection, inspect the caller for an extra `GMEInnoStartTrack(0)` after `GMEInnoOpenFileTrackW(...)`.
