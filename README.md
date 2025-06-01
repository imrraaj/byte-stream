# Bytestream

<p align=center>
  <img src="./assets/logos/bytestream.svg" alt="Bytestream Logo" width="256" />
</p>

> [!WARNING]
> Version 0.2.0 is a work in progress. It is not feature complete and may contain bugs. Please report any issues you encounter on the Issues page

This is a recreational project to create a "no bullshit video player" that has all the bloat removed. I started this project to learn more about video processing and rendering, and to create a simple, fast, and efficient video player.

## Features
- No unnecessary features
- Basic video and audio playback
- Seeking
- Pausing and resuming
- Volume control
- Fullscreen mode
- Video subtitles (ASS format currently)
- Custom Shader Support
    - Drag and drop shader files (.fs) to apply custom effects in real time!
    - **Examples of shader effects**: Grayscale, Low Brightness, Sepia, Glow, Pixelated, and more!

### Key bindings
| **Key**           | **Action**                       |
| ----------------- | -------------------------------- |
| Space             | Play / Pause                     |
| Drag & Drop (.fs) | Apply custom shader              |
| Arrow Up          | Volume Up                        |
| Arrow Down        | Volume Down                      |
| Arrow Left        | Seek 5 seconds backward          |
| Arrow Right       | Seek 5 seconds forward           |
| p (when paused)   | Take screenshot of current frame |
| b                 | Change audio language            |
| v                 | Change subtitle language         |
| u                 | Undo all shaders                 |
 
## Supported Audio Formats

- mp4
- mkv
- webm


## Build from Source

External Dependencies:
- [ffmpeg](https://ffmpeg.org/) executable available in `PATH` environment variable (**required**).
```console
$ sudo apt install ffmpeg-dev # For Debian/Ubuntu
$ brew install ffmpeg # For MacOS
```
- [nob.h](https://github.com/tsoding/nob.h/) Build System
- [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) for file dialogs
- [raylib](https://www.raylib.com/) for rendering and audio playback


### Linux and MacOS

```console
$ make
$ ./build/bytestream
$ ./build/bytestream ./video.mp4 # or pass the video path 
```

If the build fails because of missing header files, you may need to install the X11 dev packages.

On Debian, Ubuntu, etc, do this:

```console
$ sudo apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev
```

On MacOS, you may need to do this:

```console
$ xcode-select --install
```