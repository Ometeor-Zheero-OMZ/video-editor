# Video Editor - C++ Video Processing Tool

## Overview

This project implements fundamental video editing functionality using C++ and FFmpeg libraries. It's designed as a command-line tool that demonstrates low-level video processing capabilities without a graphical user interface.

## Purpose

This experimental project serves as a technical exploration of video processing concepts in C++. While a more comprehensive production-ready implementation is planned for future development in Rust, this C++ version demonstrates significant functionality and serves as a portfolio piece showcasing video processing knowledge.

## Features

The project implements the following video editing capabilities:

- Video file analysis (metadata, codec information, resolution)
- Frame-by-frame operations
- Video output generation
- Cut editing (extracting specific time ranges)
- Trimming (cropping spatial regions)
- Resizing (changing video resolution)
- Filter application (brightness, contrast, saturation adjustments)
- Video concatenation
- Transition effects (fade in/out, cross-dissolve)
- Audio processing (volume adjustment, noise reduction, BGM addition)

## Technical Implementation

This project uses:

- C++26 standard
- FFmpeg libraries (libavcodec, libavformat, libavutil, libswscale, libavfilter)
- CMake build system

## Building from Source

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

```sh
./video_codec <input_file>
```

Example:

```sh
./video_codec video.mp4
```

or

./vide_codec <video_file> <output_file> <max_frame>

```sh
./video_codec video.mp4 ./output 100
```

## Future Development

This project serves as a foundation for concepts that will be further developed in a more extensive Rust-based implementation. However, this C++ version is not a trivial demonstration - it implements substantial video processing capabilities and can be used as a functional command-line video processing tool.

## License

MIT License

## Author

itsakeyfut

Feel free to modify any sections to better match your project goals or to add more specific details about functionality as you implement more features. You can also add sections about contribution guidelines, known limitations, or acknowledgments if relevant to your project.
