#!/bin/bash
set -e

echo "🚀 Setting up ESPHome Component Development Environment..."

# Install Python tools
pip install --upgrade pip black flake8 pylint mypy pytest jupyter ipython platformio

# Update ESPHome
pip install --upgrade esphome

# Claude Code - MCP server
sudo -u esphome bash -c "curl -fsSL https://claude.ai/install.sh | bash -s latest"
sudo -u esphome bash -c "/home/esphome/.local/bin/claude mcp add code npx \"@steipete/claude-code-mcp\" \"/workspace\""
sudo pip install -r /workspace/tests/requirements.txt

# Set up USB permissions
cat > /etc/udev/rules.d/99-esphome-usb.rules << 'UDEV_EOF'
SUBSYSTEM=="usb", ATTR{idVendor}=="303a", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="10c4", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="1a86", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="0403", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="0463", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="047c", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="0483", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="04b3", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="04d8", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="050d", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="051d", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="0592", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="05dd", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="06da", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="075d", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="0764", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="09ae", MODE="0666"
SUBSYSTEM=="usb", ATTR{idVendor}=="09d6", MODE="0666"
UDEV_EOF

# Create aliases
cat >> /home/esphome/.bashrc << BASH_EOF
export PATH=~/.local/bin/:\$PATH
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
alias dclaude='claude --dangerously-skip-permissions'
BASH_EOF

# Welcome message
cat > /tmp/WELCOME.md << 'WELCOME_EOF'
# 🎉 ESPHome Component Development Environment Ready!

## Quick Commands
- `esphome-dashboard` - Start web dashboard
- `./tools/scan-usb.sh` - Check USB devices
- `./tools/generate_vendor_list.py` - Generate UPS Vendor List
- `build-component` - Build test config
- `flash-test` - Flash to device
- `monitor` - View logs

Happy coding! 🚀
WELCOME_EOF

echo "✅ Development environment setup complete!\n\n"
cat /tmp/WELCOME.md
