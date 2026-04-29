# Inno Setup wrapper

This directory adds a Windows-only `stdcall` wrapper DLL around libgme so an
Inno Setup installer can play retro game music without implementing PCM output
in Pascal Script.

## What it exports

The wrapper is a single-instance player with these exports:

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

The file loader accepts UTF-16 Windows paths and reads the music file with
`CreateFileW`, then passes the bytes to `gme_open_data()`. This avoids ANSI path
problems in the original `gme_open_file()` API.

`GMEInnoOpenFileW()` keeps the original default behavior and starts the default
track. `GMEInnoOpenFileTrackW()` lets Inno Setup choose a specific sub-track,
which is useful for formats such as NSF that can contain multiple songs.

## Build

Use a 32-bit Windows build if you want maximum compatibility with current Inno
Setup releases.

```powershell
cmake -S . -B build -DGME_BUILD_SHARED=OFF -DGME_BUILD_INNO_DLL=ON
cmake --build build --config Release --target gme_inno
```

## Inno Setup usage

See [example.iss](/D:/Codex/libgme_inno/inno/example.iss) for a minimal script.
It uses the `files:` loader syntax documented by Inno Setup and calls the DLL
through `stdcall`.
