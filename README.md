# Matrix Filter

A virtual camera application that passes video through from a real camera with random Matrix-style glitch effects at configurable intervals. Perfect for adding some Neo-style flair to your long-running video calls.

## A Very Poor Demo
[Screencast_20260109_092245.webm](https://github.com/user-attachments/assets/13f2ed61-305e-4c9a-81d1-0c7768e04039)


## Features

- Passthrough video from any V4L2 camera to a virtual camera device
- **On-demand camera access** - physical camera only opens when virtual camera is in use
- Random trigger timing (configurable min/max interval)
- TV static effect transition
- Matrix-style falling character animation with Japanese Katakana
- Configurable effect duration and cycle count
- Human-readable time formats (e.g., `500ms`, `5s`, `2m`, `1h`)
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

Time values accept units: ms, s, m, h (e.g., 500ms, 5s, 2m, 1h)

Options:
  -d, --device <path>        Input camera device (default: auto-detect)
  -o, --output <path>        Virtual camera device (default: /dev/video2)
  -r, --res <level>          Resolution: high, medium, low (default: high)
  --min-interval <time>      Minimum interval between effects (default: 1m)
  --max-interval <time>      Maximum interval between effects (default: 60m)
  --effect-duration <time>   Matrix effect duration (default: 5s)
  --static-duration <time>   Static effect duration (default: 300ms)
  --start-delay <time>       Initial delay before first effect (default: random)
  -c, --cycles <count>       Number of effect cycles, 0=infinite (default: 0)
  -t, --test                 Trigger effect immediately (same as --start-delay 0)
  --no-on-demand             Keep camera open always (don't wait for consumers)
  -h, --help                 Show this help
```

## On-Demand Mode

By default, matrix-filter runs in **on-demand mode**:

- The physical camera is only opened when an application connects to the virtual camera
- When no app is using the virtual camera, the physical camera is released
- This allows other applications (like direct video calls) to use the camera
- Static frames are displayed while the camera initializes (~1-2 seconds)
- If the camera is busy (another app has it), matrix-filter will poll until it becomes available

Use `--no-on-demand` to disable this and keep the camera open at all times.

## Example Configurations

### Quick Test Mode
Trigger the effect immediately to test it works:
```bash
./matrix-filter --test --effect-duration 500ms --static-duration 300ms
```

### One-Shot Mode
Run the effect once then just passthrough video forever:
```bash
./matrix-filter --test --cycles 1 --effect-duration 8s
```

### Prank Mode (Frequent Effects)
Effect triggers randomly every 1-5 minutes:
```bash
./matrix-filter --min-interval 1m --max-interval 5m
```

### Subtle Mode (Rare Effects)
Effect triggers randomly every 30-60 minutes for surprise factor:
```bash
./matrix-filter --min-interval 30m --max-interval 1h --effect-duration 3s
```

### Meeting Ender
Run 3 cycles of the effect then return to normal video:
```bash
./matrix-filter --min-interval 5m --max-interval 10m --cycles 3
```

### Long Matrix Effect
Extended matrix waterfall for dramatic effect:
```bash
./matrix-filter --test --effect-duration 15s --static-duration 1s
```

### Low Bandwidth Mode
Use lower resolution to reduce CPU/bandwidth usage:
```bash
./matrix-filter --res low
```

### Delayed Start
Wait 10 seconds before first effect, then random intervals:
```bash
./matrix-filter --start-delay 10s
```

### Always-On Camera (Disable On-Demand)
Keep the physical camera open at all times:
```bash
./matrix-filter --no-on-demand
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
2. **Static** - TV static noise (configurable duration)
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

### "Camera unavailable, polling..."
Another application has the camera open. Either:
- Close the other application
- Wait for it to release the camera (matrix-filter will detect this automatically)
- Use `--no-on-demand` if you want to fail immediately instead of polling

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
