#!/bin/bash
echo "🔍 Scanning USB devices..."
echo "========================="
echo "All USB devices:"
lsusb
echo ""
echo "ESP32 boards:"
lsusb | grep -iE "(303a|10c4|1a86)" || echo "No ESP32 boards found"
echo ""
# Auto-generated from ups_vendors.h
echo "UPS devices:"
lsusb | grep -iE "(0001|0463|050d|051d|05b8|0665|06da|0764|0925|09ae|09d6)" || echo "No UPS devices found"
echo ""
echo "Serial ports:"
ls -la /dev/tty{USB,ACM}* 2>/dev/null || echo "No serial ports found"


  