---
name: platformio
description: Skill for platformio operations like build, upload and upload the littlefs filesystem.
---

# PlatformIO Skill
This skill provides commands for building, uploading, and managing the LittleFS filesystem for the ESP32 Car project using PlatformIO. It allows you to execute common PlatformIO tasks directly from the command line, streamlining your development workflow.

## Commands
- `~/.platformio/penv/bin/pio build`: Compiles the project and generates the firmware binary.
- `~/.platformio/penv/bin/pio upload`: Uploads the compiled firmware to the ESP32 device.
- `~/.platformio/penv/bin/pio uploadfs`: Uploads the LittleFS filesystem to the ESP32 device, allowing you to serve static files for the web interface.
- `~/.platformio/penv/bin/pio clean`: Cleans the build environment, removing all generated files.
- `~/.platformio/penv/bin/pio run --target upload`: Combines the build and upload steps into a single command for convenience.
