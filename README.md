# Vela — a VLC-inspired media player

Vela is a lightweight, native desktop video player written in **C++ / Qt 6**
and powered by **libmpv** (the engine behind mpv, built on FFmpeg). Because it
uses the FFmpeg decoding stack, it plays essentially every common format out of
the box — **MP4, MKV, AVI, MOV, WebM, FLV, WMV, MPEG, TS, M4V**, and more — plus
network streams (`http`, `https`, `rtsp`, …).

It compiles to a single native binary you can install and set as your system's
default video player.

## Features

- Plays MP4, MKV, AVI, MOV, WebM, FLV, WMV, MPEG/TS, OGV and anything else FFmpeg supports
- Hardware-accelerated decoding when available (`hwdec=auto-safe`)
- Play / pause, stop, seek bar with time readout
- Volume slider + mute, up to 130% boost
- Playlist with previous / next and auto-advance at end of file
- **Automatic subtitle download** from OpenSubtitles, with sync-matched results
- Manual subtitle file loading, track switching, and on-the-fly delay adjustment
- Fullscreen toggle
- Drag & drop files onto the window
- Open local files or network URLs / streams
- Command-line file arguments, so the OS can launch it to open a video
- Full keyboard control (see below)
- Desktop integration so it appears in your launcher and "Open With" menus

## Keyboard shortcuts

| Key            | Action            | Key        | Action            |
| -------------- | ----------------- | ---------- | ----------------- |
| `Space`        | Play / Pause      | `F`        | Fullscreen        |
| `S`            | Stop              | `Esc`      | Exit fullscreen   |
| `→` / `←`      | Seek ±10s         | `Up`/`Down`| Volume ±5         |
| `Shift+→`/`←`  | Seek ±60s         | `M`        | Mute              |
| `N` / `P`      | Next / Previous   | `Ctrl+O`   | Open file(s)      |
| `Ctrl+D`       | Download subtitles| `V`        | Subtitles on/off  |
| `J`            | Cycle sub track   | `Z`/`Shift+Z`| Subtitle delay ∓ |

## Subtitles

Vela can fetch subtitles automatically from **OpenSubtitles.com**:

1. **Set up once** — open *Subtitles → OpenSubtitles Account…* and paste a free
   API key (create an account at opensubtitles.com, then **API → Consumers**).
   Add your username/password too, since downloads require a logged-in account.
   Set your preferred languages (e.g. `en,es,fr`).
2. **While a video plays**, press `Ctrl+D` (or *Subtitles → Download
   Subtitles…*). Vela searches and shows what it found.
3. **Pick one and download.** The file is saved next to your video and loaded
   instantly.

### Will they be in sync?

Mostly yes — and that's by design. Vela fingerprints the video with the
**OpenSubtitles "movie hash"** (computed from the file's size and its first/last
64 KB) and searches by it first. Results that match the hash were timed to that
exact release, so they line up without any fiddling — those are marked with a
**✓** in the *Sync* column. Vela also does a looser filename search to give you
more language choices; those aren't sync-guaranteed, so if one is slightly off,
nudge it live with `Z` (earlier) and `Shift+Z` (later) in 0.1 s steps. That's
the same idea as VLC's subtitle-delay controls.

> Credentials are stored locally via Qt's settings (plain config file). Use a
> throwaway OpenSubtitles account if that concerns you.

## Build & install

### Linux (recommended path)

Install the dependencies, then build:

```bash
# Debian / Ubuntu
sudo apt install build-essential cmake pkg-config qt6-base-dev libmpv-dev

# Fedora
sudo dnf install gcc-c++ cmake pkgconf-pkg-config qt6-qtbase-devel mpv-libs-devel

# Arch
sudo pacman -S base-devel cmake qt6-base mpv
```

Then from the project root:

```bash
./build.sh            # builds ./build/vela
./build.sh install    # builds, installs system-wide, registers the .desktop file
```

`./build.sh install` puts the binary at `/usr/local/bin/vela`, installs the
desktop entry and icon, and refreshes the desktop/icon caches.

### Set Vela as your default video player (Linux)

After installing, either:

```bash
packaging/set-default.sh
```

or do it manually for the formats you care about:

```bash
xdg-mime default vela.desktop video/mp4 video/x-matroska video/x-msvideo
```

or simply right-click any video → **Properties → Open With → Vela → Set as default**.

### Manual build (without the script)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build
```

### macOS

```bash
brew install cmake qt mpv pkg-config
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
```

The binary lands at `build/vela`. To set it as the default player, use Finder →
right-click a video → **Get Info → Open with → Other… → vela → Change All**.

### Windows

Use [MSYS2](https://www.msys2.org/) (UCRT64 shell):

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-mpv \
          mingw-w64-ucrt-x86_64-pkgconf
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This produces `build\vela.exe`. To run it outside the MSYS2 shell (e.g. by
double-clicking), it needs its runtime DLLs next to it. From the UCRT64 shell:

```bash
# Qt runtime DLLs + platform plugins
windeployqt6 build/vela.exe
# libmpv and its dependencies
cp /ucrt64/bin/libmpv-2.dll build/
```

If a launched copy still reports a missing DLL, copy the named file from
`C:\msys64\ucrt64\bin` next to `vela.exe`. Then set the default via
**Settings → Apps → Default apps**, or right-click a video →
**Open with → Choose another app → vela → Always**.

## Project layout

```
CMakeLists.txt          Build definition (Qt6 + libmpv, cross-platform)
build.sh                One-shot configure/build/install helper
src/
  main.cpp              App entry, CLI args, locale setup for mpv
  MainWindow.*          Window, menus, playlist, shortcuts, drag & drop
  MpvWidget.*           libmpv playback engine embedded in a native window
  PlayerControls.*      Transport bar: play/seek/volume/fullscreen
  SubtitleManager.*     OpenSubtitles search/download + movie-hash matching
  SubtitleDialogs.*     Subtitle results picker and account settings dialogs
resources/
  vela.desktop         Desktop entry with video MIME associations
  resources.qrc        Qt resource bundle (app icon)
  icons/vela.svg       Application icon
packaging/
  set-default.sh       Registers Vela as the default video handler (Linux)
```

## How it works

`MpvWidget` creates a native window and hands its handle to libmpv (the `wid`
embedding strategy), so mpv renders decoded video directly into the Qt widget.
Player state (position, duration, pause, volume, title) is observed through
mpv's property API and surfaced as Qt signals, which `MainWindow` wires to the
`PlayerControls` bar and the window title. All decoding/demuxing is handled by
mpv/FFmpeg, which is why the format support is so broad.

## License

Provided as-is for personal use. libmpv and FFmpeg are licensed under the
LGPL/GPL — see their respective projects.
