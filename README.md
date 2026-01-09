# Matrix Filter

A virtual camera application that passes video through from a real camera with random Matrix-style glitch effects at configurable intervals. Perfect for adding some Neo-style flair to your video calls.

## Features

- Passthrough video from any V4L2 camera to a virtual camera device
- Random trigger timing (configurable min/max interval)
- TV static effect transition
- Matrix-style falling character animation with Japanese Katakana
- Configurable effect duration and cycle count
- Works with any application that uses V4L2 (Zoom, OBS, Teams, etc.)

## Prerequisites

### System Requirements
- Linux with V4L2 support
- OpenCV 4.x
- FreeType 2 (for Japanese character rendering)
- CMake 3.10+
- C++17 compiler
- A Japanese font (Noto Sans CJK recommended)

### Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt-get install libopencv-dev libfreetype-dev v4l2loopback-dkms cmake build-essential fonts-noto-cjk
```

**Arch Linux:**
```bash
sudo pacman -S opencv freetype2 v4l2loopback-dkms cmake noto-fonts-cjk
```

## Building

```bash
cd /ARCHIVE/Programming/matrix-filter
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Setup Virtual Camera

Load the v4l2loopback kernel module:

```bash
sudo modprobe v4l2loopback devices=1 video_nr=2 card_label="Matrix Filter"
```

To make it persistent across reboots, add to `/etc/modules-load.d/v4l2loopback.conf`:
```
v4l2loopback
```

And configure in `/etc/modprobe.d/v4l2loopback.conf`:
```
options v4l2loopback devices=1 video_nr=2 card_label="Matrix Filter"
```

## Usage

```
./matrix-filter [OPTIONS]

Options:
  -d, --device <path>        Input camera device (default: auto-detect)
  -o, --output <path>        Virtual camera device (default: /dev/video2)
  --min-interval <minutes>   Minimum interval between effects (default: 1)
  --max-interval <minutes>   Maximum interval between effects (default: 60)
  --effect-duration <ms>     Matrix effect duration in milliseconds (default: 5000)
  --static-frames <count>    Static frames before matrix effect (default: 10)
  -c, --cycles <count>       Number of effect cycles, 0=infinite (default: 0)
  -t, --test                 Trigger effect immediately on start
  -h, --help                 Show this help
```

## Example Configurations

### Quick Test Mode
Trigger the effect immediately to test it works:
```bash
./matrix-filter --test --effect-duration 5000
```

### One-Shot Mode
Run the effect once then just passthrough video forever:
```bash
./matrix-filter --test --cycles 1 --effect-duration 8000
```

### Prank Mode (Frequent Effects)
Effect triggers randomly every 1-5 minutes:
```bash
./matrix-filter --min-interval 1 --max-interval 5
```

### Subtle Mode (Rare Effects)
Effect triggers randomly every 30-60 minutes for surprise factor:
```bash
./matrix-filter --min-interval 30 --max-interval 60 --effect-duration 3000
```

### Meeting Ender
Run 3 cycles of the effect then return to normal video:
```bash
./matrix-filter --min-interval 5 --max-interval 10 --cycles 3
```

### Long Matrix Effect
Extended matrix waterfall for dramatic effect:
```bash
./matrix-filter --test --effect-duration 15000 --static-frames 30
```

### View the Virtual Camera
```bash
# Using ffplay
ffplay /dev/video2

# Using mpv
mpv av://v4l2:/dev/video2

# Or select "Matrix Filter" as camera in Zoom/OBS/Teams/etc.
```

## Effect Sequence

1. **Passthrough** - Normal camera video
2. **Static** - TV static noise (configurable frames)
3. **Matrix** - Falling green Katakana characters (configurable duration)
4. **Return to Passthrough** (or stop if cycles complete)

## Troubleshooting

### "Virtual camera device not found"
Make sure v4l2loopback is loaded:
```bash
sudo modprobe v4l2loopback devices=1 video_nr=2
v4l2-ctl --list-devices
```

### "No camera detected"
Check available cameras:
```bash
v4l2-ctl --list-devices
ls -la /dev/video*
```

### "Failed to load Japanese font"
Install a CJK font:
```bash
# Ubuntu/Debian
sudo apt-get install fonts-noto-cjk

# Arch
sudo pacman -S noto-fonts-cjk
```

### Permission denied
Add your user to the video group:
```bash
sudo usermod -aG video $USER
# Log out and back in
```

### Effect looks choppy
The matrix animation runs at ~33 FPS. If your system is under load, try reducing camera resolution or closing other applications.

## License

MIT
