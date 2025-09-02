#!/bin/bash
# Script to collect debug information for protocol implementation bug reports
# Usage: ./debug-protocol-issue.sh <esp32_ip> [ups_name]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check arguments
if [ $# -eq 0 ]; then
    echo -e "${RED}Error: ESP32 IP address required${NC}"
    echo "Usage: $0 <esp32_ip> [ups_name]"
    echo "Example: $0 192.168.1.100"
    echo "         $0 192.168.1.100 my_ups"
    exit 1
fi

ESP32_IP=$1
UPS_NAME=${2:-"ups"}  # Default to "ups" if not provided
OUTPUT_DIR="protocol-bug-$(date +%Y%m%d-%H%M%S)"
OUTPUT_FILE="$OUTPUT_DIR/bug-report.md"

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo -e "${GREEN}=== UPS Protocol Bug Debug Collection ===${NC}"
echo -e "ESP32 IP: ${YELLOW}$ESP32_IP${NC}"
echo -e "UPS Name: ${YELLOW}$UPS_NAME${NC}"
echo "Output directory: $OUTPUT_DIR"
echo ""

# Function to capture with timeout and error handling
capture_with_retry() {
    local description=$1
    local command=$2
    local output_file=$3
    local retry_count=3
    
    echo -e "${YELLOW}Capturing: $description${NC}"
    
    for i in $(seq 1 $retry_count); do
        if timeout 10 bash -c "$command" > "$output_file" 2>&1; then
            echo -e "${GREEN}✓ Success${NC}"
            return 0
        else
            if [ $i -lt $retry_count ]; then
                echo -e "${YELLOW}  Retry $i/$retry_count...${NC}"
                sleep 2
            else
                echo -e "${RED}✗ Failed after $retry_count attempts${NC}"
                echo "Failed to capture" > "$output_file"
                return 1
            fi
        fi
    done
}

# Start report
echo "# UPS Protocol Bug Report" > "$OUTPUT_FILE"
echo "Date: $(date)" >> "$OUTPUT_FILE"
echo "ESP32 IP: $ESP32_IP" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# 1. Test ESP32 connectivity
echo -e "\n${GREEN}Step 1: Testing ESP32 Connectivity${NC}"
if ping -c 1 -W 2 $ESP32_IP > /dev/null 2>&1; then
    echo -e "${GREEN}✓ ESP32 is reachable${NC}"
    echo "## ESP32 Status" >> "$OUTPUT_FILE"
    echo "ESP32 is reachable at $ESP32_IP" >> "$OUTPUT_FILE"
else
    echo -e "${RED}✗ Cannot reach ESP32 at $ESP32_IP${NC}"
    echo "Cannot reach ESP32 - check IP address and network connection" >> "$OUTPUT_FILE"
    exit 1
fi

# 2. Capture ESPHome Web API Status
echo -e "\n${GREEN}Step 2: Fetching Device Information${NC}"
echo "## Device Information" >> "$OUTPUT_FILE"
echo '```json' >> "$OUTPUT_FILE"

# Try to get device info via curl (ESPHome native API is on port 6053)
if curl -s "http://$ESP32_IP" | grep -q "ESPHome"; then
    echo -e "${GREEN}✓ ESPHome Web Server detected${NC}"
    
    # Get basic device info from web interface
    curl -s "http://$ESP32_IP/json/state" > "$OUTPUT_DIR/device_state.json" 2>/dev/null || true
    
    if [ -f "$OUTPUT_DIR/device_state.json" ] && [ -s "$OUTPUT_DIR/device_state.json" ]; then
        cat "$OUTPUT_DIR/device_state.json" >> "$OUTPUT_FILE"
    fi
else
    echo "ESPHome web server not detected or not enabled" >> "$OUTPUT_FILE"
fi
echo '```' >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# 3. Monitor real-time logs
echo -e "\n${GREEN}Step 3: Capturing Real-time Logs${NC}"
echo -e "${YELLOW}Attempting to capture ESPHome logs...${NC}"
echo -e "${BLUE}Note: This requires 'esphome' CLI tool and API enabled in your config${NC}"
echo "" 

echo "## ESPHome Logs" >> "$OUTPUT_FILE"
echo '```' >> "$OUTPUT_FILE"

# Try to capture logs using esphome CLI if available
if command -v esphome &> /dev/null; then
    echo "Attempting to connect to ESPHome logs..." >> "$OUTPUT_FILE"
    timeout 30 esphome logs --device $ESP32_IP 2>&1 | tee "$OUTPUT_DIR/esphome_logs.txt" | tail -100 >> "$OUTPUT_FILE" || true
else
    echo "ESPHome CLI not installed - cannot capture logs automatically" >> "$OUTPUT_FILE"
    echo -e "${YELLOW}Install with: pip install esphome${NC}"
fi
echo '```' >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# 4. Interactive state testing
echo -e "\n${GREEN}Step 4: Interactive State Testing${NC}"
echo -e "${YELLOW}We'll now test different UPS states interactively${NC}"
echo ""

test_state() {
    local state_name=$1
    local instruction=$2
    
    echo -e "${YELLOW}$instruction${NC}"
    echo -n "Press ENTER when ready to capture $state_name state: "
    read
    
    echo "## State: $state_name" >> "$OUTPUT_FILE"
    echo "Timestamp: $(date)" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
    
    # Capture current sensor values if web API is available
    if curl -s "http://$ESP32_IP/json/state" > "$OUTPUT_DIR/${state_name}_state.json" 2>/dev/null; then
        echo "### Sensor Values" >> "$OUTPUT_FILE"
        echo '```json' >> "$OUTPUT_FILE"
        # Extract UPS-related sensors
        cat "$OUTPUT_DIR/${state_name}_state.json" | python3 -m json.tool 2>/dev/null | grep -A2 -B2 -i "ups\|battery\|power\|voltage\|runtime\|load" >> "$OUTPUT_FILE" 2>/dev/null || echo "Could not parse sensor data" >> "$OUTPUT_FILE"
        echo '```' >> "$OUTPUT_FILE"
    fi
    
    echo "" >> "$OUTPUT_FILE"
    echo -e "${GREEN}✓ $state_name state captured${NC}"
}

# Test different states
echo -e "${GREEN}Testing UPS states...${NC}"
test_state "normal" "Ensure UPS is in NORMAL state (plugged in, battery charged)"
test_state "battery" "Now UNPLUG the UPS from AC power (running on battery)"
test_state "charging" "Now PLUG the UPS back into AC power (charging)"

# 5. Problem reproduction
echo -e "\n${GREEN}Step 5: Problem Reproduction${NC}"
echo -e "${YELLOW}Now reproduce the specific issue you're experiencing${NC}"
echo -n "Press ENTER when you're ready to start reproducing the issue: "
read

echo "## Problem Reproduction" >> "$OUTPUT_FILE"
echo "Timestamp: $(date)" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

echo -e "${YELLOW}Reproduce the issue now. Press ENTER when the issue has occurred: ${NC}"
read

# Capture state during issue
if curl -s "http://$ESP32_IP/json/state" > "$OUTPUT_DIR/issue_state.json" 2>/dev/null; then
    echo "### Sensor Values During Issue" >> "$OUTPUT_FILE"
    echo '```json' >> "$OUTPUT_FILE"
    cat "$OUTPUT_DIR/issue_state.json" | python3 -m json.tool 2>/dev/null >> "$OUTPUT_FILE" || cat "$OUTPUT_DIR/issue_state.json" >> "$OUTPUT_FILE"
    echo '```' >> "$OUTPUT_FILE"
fi

# 6. Ask for issue description
echo -e "\n${GREEN}Step 6: Issue Description${NC}"
echo "Please describe what went wrong (press Ctrl+D when done):"
echo "### Issue Description" >> "$OUTPUT_FILE"
echo '```' >> "$OUTPUT_FILE"
cat >> "$OUTPUT_FILE"
echo '```' >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# 7. Generate analysis
echo -e "\n${GREEN}Step 7: Analyzing Collected Data${NC}"

echo "## Analysis Summary" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Check for common issues in logs
if [ -f "$OUTPUT_DIR/esphome_logs.txt" ]; then
    echo "### Log Analysis" >> "$OUTPUT_FILE"
    
    # Check for USB errors
    if grep -q "USB.*error\|timeout\|disconnect" "$OUTPUT_DIR/esphome_logs.txt"; then
        echo "- ⚠️ USB communication errors detected" >> "$OUTPUT_FILE"
    fi
    
    # Check for HID errors
    if grep -q "HID.*error\|Report.*fail\|parse.*error" "$OUTPUT_DIR/esphome_logs.txt"; then
        echo "- ⚠️ HID protocol errors detected" >> "$OUTPUT_FILE"
    fi
    
    # Check for memory issues
    if grep -q "heap\|stack\|memory\|allocation" "$OUTPUT_DIR/esphome_logs.txt"; then
        echo "- ⚠️ Potential memory issues detected" >> "$OUTPUT_FILE"
    fi
    
    echo "" >> "$OUTPUT_FILE"
fi

# Compare states if JSON files exist
if [ -f "$OUTPUT_DIR/normal_state.json" ] && [ -f "$OUTPUT_DIR/battery_state.json" ]; then
    echo "### State Comparison" >> "$OUTPUT_FILE"
    echo "Differences between normal and battery states detected" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
fi

# 8. Create summary
echo -e "\n${GREEN}Creating Summary Report${NC}"

cat > "$OUTPUT_DIR/SUMMARY.md" << EOF
# Protocol Bug Report Summary

## Device Information
- ESP32 IP: $ESP32_IP
- Date: $(date)
- UPS Component: $UPS_NAME

## Files Collected
$(ls -1 $OUTPUT_DIR/*.json $OUTPUT_DIR/*.txt 2>/dev/null | xargs -I {} basename {} | sed 's/^/- /')

## States Tested
- Normal operation (AC power)
- Battery operation
- Charging state
- Issue reproduction state

## Next Steps
1. Review the bug report at: $OUTPUT_FILE
2. Check collected JSON states for anomalies
3. Create an issue at: https://github.com/oleander/esphome-components/issues
4. Use the "Protocol Implementation Bug Report" template
5. Attach relevant files from $OUTPUT_DIR/

## Quick Diagnostics
- ESP32 Reachable: $(ping -c 1 -W 2 $ESP32_IP > /dev/null 2>&1 && echo "✓ Yes" || echo "✗ No")
- Web API Available: $(curl -s "http://$ESP32_IP" | grep -q "ESPHome" && echo "✓ Yes" || echo "✗ No")
- Logs Captured: $([ -f "$OUTPUT_DIR/esphome_logs.txt" ] && echo "✓ Yes" || echo "✗ No")
EOF

echo -e "\n${GREEN}=== Debug Collection Complete ===${NC}"
echo ""
echo "Output directory: $OUTPUT_DIR/"
echo "Main report: $OUTPUT_FILE"
echo "Summary: $OUTPUT_DIR/SUMMARY.md"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Review the collected data in $OUTPUT_DIR/"
echo "2. Create an issue at: https://github.com/oleander/esphome-components/issues"
echo "3. Use the 'Protocol Implementation Bug Report' template"
echo "4. Attach the bug report and relevant JSON files"
echo ""
echo -e "${GREEN}Thank you for helping improve the UPS HID component!${NC}"

exit 0