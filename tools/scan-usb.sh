#!/bin/bash
echo "🔍 Scanning USB devices..."
echo "========================="
echo "All USB devices:"
lsusb
echo ""
echo "ESP32 boards:"
lsusb | grep -E "(303a|10c4|1a86)" || echo "No ESP32 boards found"
echo ""
echo "UPS devices:"
lsusb | grep -E "(051d|0764)" || echo "No UPS devices found"
echo ""
echo "Serial ports:"
ls -la /dev/tty{USB,ACM}* 2>/dev/null || echo "No serial ports found"
