#!/bin/bash
set -e

echo "🚀 Setting up ESPHome NUT UPS Development Environment..."

# Update and install packages
apt-get update
apt-get install -y \
    usbutils libusb-1.0-0-dev libudev-dev build-essential cmake ninja-build \
    git wget curl tree vim nano htop tmux screen jq socat minicom picocom udev

# Install Python tools
pip install --upgrade pip black flake8 pylint mypy pytest jupyter ipython platformio

# Update ESPHome
pip install --upgrade esphome

# Set up USB permissions
cat > /etc/udev/rules.d/99-esphome-usb.rules << 'UDEV_EOF'
SUBSYSTEM=="usb", ATTR{idVendor}=="303a", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="10c4", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="1a86", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="0403", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="0001", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="0463", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="051d", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="0665", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="06da", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="0764", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="09ae", MODE="0666"
UDEV_EOF

# Create aliases
cat >> ~/.bashrc << 'BASH_EOF'
alias esphome-config='esphome config'
alias esphome-compile='esphome compile'
alias esphome-upload='esphome upload'
alias esphome-logs='esphome logs'
alias esphome-dashboard='esphome dashboard /workspace'
alias lsusb='lsusb -v'
alias serialports='ls -la /dev/tty{USB,ACM}* 2>/dev/null || echo "No serial ports found"'
alias build-component='cd /workspace && esphome compile full-test.yaml'
alias flash-test='cd /workspace && esphome upload full-test.yaml'
alias monitor='cd /workspace && esphome logs full-test.yaml'
BASH_EOF

# Welcome message
cat > /tmp/WELCOME.md << 'WELCOME_EOF'
# 🎉 ESPHome NUT UPS Development Environment Ready!

## Quick Commands
- `esphome-dashboard` - Start web dashboard
- `./tools/scan-usb.sh` - Check USB devices
- `build-component` - Build test config
- `flash-test` - Flash to device
- `monitor` - View logs

Happy coding! 🚀
WELCOME_EOF

echo "✅ Development environment setup complete!"
cat /tmp/WELCOME.md
