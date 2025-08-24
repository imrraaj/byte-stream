# Bytestream

<p align=center>
  <img src="./assets/logos/bytestream.svg" alt="Bytestream Logo" width="256" />
</p>

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)]()
[![License](https://img.shields.io/badge/license-MIT-blue)]()
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20-lightgrey)]()

> [!WARNING]
> Version 0.2.0 is a work in progress. It is not feature complete and may contain bugs. Please report any issues you encounter on the Issues page

A minimal, high-performance video player built in C with no unnecessary bloat. Bytestream focuses on essential video playback functionality with custom shader support and clean, responsive UI.

## Why Bytestream?

- **Minimal footprint** - No unnecessary features or dependencies
- **Custom shaders** - Real-time visual effects via drag-and-drop
- **Cross-platform** - Native builds for Linux, macOS, and Windows

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


## Project Structure

```
bytestream/
├── src/                   # Source code (.c/.h files)
├── external/              # External source dependencies (compiled with project)
├── include/               # Headers for pre-compiled libraries
├── lib/                   # Pre-compiled libraries (.a/.so/.dll files)
└── assets/                # Resources (icons, fonts, logos)
```

### Dependency Organization Rules:
- **`external/`** - Source code compiled with project (small dependencies)
- **`include/` + `lib/`** - Pre-compiled libraries link against (large dependencies)
- **`assets/`** - Static resources (images, fonts, shaders)

## Build from Source

### Prerequisites

**System Requirements:**
- C compiler (GCC or Clang)
- Make build system
- pkg-config for dependency management

**Required Libraries:**
- [FFmpeg development libraries](https://ffmpeg.org/) (**required**)
  ```bash
  # Debian/Ubuntu
  sudo apt install libavformat-dev libavcodec-dev libavutil-dev libswresample-dev libswscale-dev
  
  # Fedora/RHEL
  sudo dnf install ffmpeg-devel
  
  # macOS
  brew install ffmpeg
  
  # Arch Linux
  sudo pacman -S ffmpeg
  ```

**Platform-specific Dependencies:**
```bash
# Linux - X11 development headers
sudo apt install libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev

# macOS - Xcode command line tools
xcode-select --install
```

### Quick Start

```bash
# Clone the repository
git clone <repository-url>
cd bytestream

# Build the project
make

# Run the player
./build/bytestream                    # Open file dialog
./build/bytestream ./video.mp4       # Direct file playback
```

### Build Targets

```bash
make                    # Debug build
make release           # Optimized release build (platform-specific)
make clean             # Clean all build artifacts
make clean-fast        # Clean main binary (keep external deps)
```

### Dependencies Included

The project includes all necessary dependencies:

**Pre-compiled Libraries** (`lib/` + `include/`):
- [Raylib](https://www.raylib.com/) - Graphics and audio framework
- Platform-specific builds for Linux/macOS/Windows

**Source Dependencies** (`external/`):
- [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) - Native file dialogs
- [nob.h](https://github.com/tsoding/nob.h/) - Build system utilities

**Runtime Dependencies:**
- FFmpeg libraries (dynamically linked)

### Troubleshooting

**Build Issues:**
- Ensure all FFmpeg development packages are installed
- On older systems, you may need to update pkg-config
- For missing X11 headers on Linux, install the development packages listed above

**Runtime Issues:**
- Ensure FFmpeg is properly installed and accessible
- Check that your video files are in supported formats
- Verify graphics drivers support OpenGL 3.3+

## Development

### Architecture Overview

Bytestream follows a modular architecture with clear separation of concerns:

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Application   │    │     Player      │    │    Decoder      │
│   (main.c)      │───▶│   (player.c)    │───▶│  (decoder.c)    │
│                 │    │                 │    │                 │
│ • Entry point   │    │ • UI rendering  │    │ • FFmpeg wrapper│
│ • File handling │    │ • Input events  │    │ • Video/Audio   │
│ • App lifecycle │    │ • State mgmt    │    │ • Threading     │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

### Key Components

- **Decoder** - FFmpeg integration for media processing
- **Player** - UI, input handling, and playback control  
- **Application** - Font management and app-wide state
- **Queue** - Thread-safe video frame buffering
- **Subtitle** - Text subtitle rendering and timing

### Code Style

- **C99 standard** with minimal extensions
- **Snake_case** for functions and variables
- **PascalCase** for types and structs
- **No global variables** - state passed via struct pointers
- **Memory safety** - Explicit cleanup and error handling

### Adding Features

1. **Media features** → Extend `decoder.c` and `player.c`
2. **UI features** → Modify `player.c` UI rendering
3. **Platform features** → Add to `external/` or system integration
4. **Build features** → Update `Makefile` and `generator.c`

### Performance Considerations

- **Multi-threaded** - Separate decode, video, and audio threads
- **Double buffering** - Smooth video rendering without tearing
- **Memory pools** - Efficient frame buffer management
- **Minimal allocations** - Reuse resources where possible

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow the existing code style and patterns
4. Test on your target platform
5. Submit a pull request

### Testing

```bash
# Test with various formats
./build/bytestream test.mp4
./build/bytestream test.mkv  
./build/bytestream test.webm

# Test shader loading
# Drag and drop .fs files onto the window while playing
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- [FFmpeg](https://ffmpeg.org/) - Media processing framework
- [Raylib](https://www.raylib.com/) - Graphics and audio library
- [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) - Cross-platform file dialogs