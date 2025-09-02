#!/bin/bash
# Script to collect debug information for new UPS protocol requests
# Usage: ./collect-protocol-debug.sh <ups_name>

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if UPS name is provided
if [ $# -eq 0 ]; then
    echo -e "${RED}Error: UPS name required${NC}"
    echo "Usage: $0 <ups_name>"
    echo "Example: $0 myups"
    echo ""
    echo "First configure your UPS in /etc/nut/ups.conf:"
    echo "[myups]"
    echo "    driver = usbhid-ups"
    echo "    port = auto"
    echo "    vendorid = 051d"
    echo "    productid = 0002"
    exit 1
fi

UPS_NAME=$1
OUTPUT_DIR="ups-protocol-debug-$(date +%Y%m%d-%H%M%S)"
OUTPUT_FILE="$OUTPUT_DIR/debug-output.md"

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo -e "${GREEN}=== UPS Protocol Debug Collection Tool ===${NC}"
echo -e "Collecting debug information for: ${YELLOW}$UPS_NAME${NC}"
echo "Output directory: $OUTPUT_DIR"
echo ""

# Function to run command and save output
run_and_save() {
    local description=$1
    local command=$2
    local output_file=$3
    
    echo -e "${YELLOW}Running: $description${NC}"
    echo "## $description" >> "$OUTPUT_FILE"
    echo '```bash' >> "$OUTPUT_FILE"
    echo "# Command: $command" >> "$OUTPUT_FILE"
    
    if eval "$command" > "$output_file" 2>&1; then
        cat "$output_file" >> "$OUTPUT_FILE"
        echo -e "${GREEN}✓ Success${NC}"
    else
        echo "Error running command" >> "$OUTPUT_FILE"
        echo -e "${RED}✗ Failed${NC}"
    fi
    
    echo '```' >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
}

# Start collecting information
echo "# UPS Protocol Debug Information" > "$OUTPUT_FILE"
echo "Date: $(date)" >> "$OUTPUT_FILE"
echo "UPS Name: $UPS_NAME" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# 1. USB Device Information
echo -e "\n${GREEN}Step 1: USB Device Information${NC}"
run_and_save "USB Device List" "lsusb | grep -i ups || lsusb | grep -iE 'apc|cyber|eaton|tripplite'" "$OUTPUT_DIR/lsusb.txt"

# Get vendor and product ID
VENDOR_PRODUCT=$(lsusb | grep -i ups | head -1 | sed -n 's/.*ID \([0-9a-f]*:[0-9a-f]*\).*/\1/p')
if [ -n "$VENDOR_PRODUCT" ]; then
    echo "Detected USB ID: $VENDOR_PRODUCT"
    run_and_save "USB Device Details" "lsusb -v -d $VENDOR_PRODUCT" "$OUTPUT_DIR/lsusb-verbose.txt"
fi

# 2. Test UPS driver connection
echo -e "\n${GREEN}Step 2: Testing UPS Driver Connection${NC}"
run_and_save "Driver Test" "sudo timeout 10 /lib/nut/usbhid-ups -DD -a $UPS_NAME 2>&1 | head -500" "$OUTPUT_DIR/driver-test.txt"

# 3. Collect HID paths
echo -e "\n${GREEN}Step 3: Collecting HID Paths${NC}"
run_and_save "HID Path Discovery" "sudo timeout 10 /lib/nut/usbhid-ups -DD -a $UPS_NAME 2>&1 | grep 'Path:' | sort -u" "$OUTPUT_DIR/hid-paths.txt"

# 4. Start NUT driver if not running
echo -e "\n${GREEN}Step 4: Starting NUT Driver${NC}"
sudo upsdrvctl stop $UPS_NAME 2>/dev/null || true
sleep 2
sudo upsdrvctl start $UPS_NAME
sleep 3

# 5. Collect upsc output
echo -e "\n${GREEN}Step 5: Collecting UPS Variables${NC}"
run_and_save "UPS Variables (upsc)" "upsc $UPS_NAME" "$OUTPUT_DIR/upsc-full.txt"

# 6. Collect supported commands
echo -e "\n${GREEN}Step 6: Checking Supported Commands${NC}"
run_and_save "UPS Commands" "upscmd -l $UPS_NAME" "$OUTPUT_DIR/upscmd.txt"

# 7. Collect state-specific debug output
echo -e "\n${GREEN}Step 7: Collecting State-Specific Debug Output${NC}"
echo -e "${YELLOW}This section requires manual intervention...${NC}"
echo ""

# Function for interactive debug collection
collect_state_debug() {
    local state_name=$1
    local instruction=$2
    local wait_time=${3:-30}
    
    echo -e "${YELLOW}$instruction${NC}"
    echo -e "Press ${GREEN}ENTER${NC} when ready to start capturing $state_name state..."
    read
    
    echo "Capturing $state_name state for $wait_time seconds..."
    
    # Start background capture
    sudo timeout $wait_time /lib/nut/usbhid-ups -DD -a $UPS_NAME 2>&1 | grep -A2 -B2 "Report" > "$OUTPUT_DIR/$state_name-reports.txt" &
    PID=$!
    
    # Show progress
    for i in $(seq $wait_time -1 1); do
        echo -ne "\rTime remaining: $i seconds "
        sleep 1
    done
    echo -e "\n${GREEN}✓ $state_name capture complete${NC}"
    
    # Also capture upsc output for this state
    upsc $UPS_NAME > "$OUTPUT_DIR/$state_name-upsc.txt"
    
    # Add to main output file
    echo "## Debug Output - $state_name State" >> "$OUTPUT_FILE"
    echo '```bash' >> "$OUTPUT_FILE"
    cat "$OUTPUT_DIR/$state_name-reports.txt" | head -100 >> "$OUTPUT_FILE"
    echo '```' >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
    
    echo "## UPS Variables - $state_name State" >> "$OUTPUT_FILE"
    echo '```bash' >> "$OUTPUT_FILE"
    cat "$OUTPUT_DIR/$state_name-upsc.txt" >> "$OUTPUT_FILE"
    echo '```' >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
}

# Collect different states
echo -e "${GREEN}We'll now collect debug data for different UPS states.${NC}"
echo -e "${YELLOW}IMPORTANT: Only proceed if it's safe to disconnect AC power!${NC}"
echo ""

collect_state_debug "online" "Ensure UPS is connected to AC power (normal operation)" 30
collect_state_debug "battery" "Now UNPLUG the UPS from AC power (running on battery)" 30
collect_state_debug "charging" "Now PLUG the UPS back into AC power (charging mode)" 30

# 8. Optional: Low battery test
echo -e "\n${YELLOW}Optional: Low Battery Test${NC}"
echo "Would you like to test low battery state? This requires running on battery until low battery warning."
echo -n "This may take a long time and is optional. Continue? (y/N): "
read -r response
if [[ "$response" =~ ^[Yy]$ ]]; then
    collect_state_debug "lowbatt" "Keep UPS on battery until low battery warning appears" 60
fi

# 9. Analyze collected data
echo -e "\n${GREEN}Step 8: Analyzing Collected Data${NC}"
echo "## Report ID Analysis" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# Extract unique report IDs
echo "### Discovered Report IDs" >> "$OUTPUT_FILE"
echo '```' >> "$OUTPUT_FILE"
grep -h "Report" "$OUTPUT_DIR"/*-reports.txt 2>/dev/null | \
    sed -n 's/.*Report \([0-9a-fA-F]*\).*/0x\1/p' | \
    sort -u | while read report_id; do
    echo "Report ID: $report_id"
    # Find example data for this report
    grep -h "Report ${report_id#0x}" "$OUTPUT_DIR"/*-reports.txt 2>/dev/null | head -1
done >> "$OUTPUT_FILE"
echo '```' >> "$OUTPUT_FILE"

# 10. Create summary
echo -e "\n${GREEN}Step 9: Creating Summary${NC}"
cat > "$OUTPUT_DIR/SUMMARY.md" << EOF
# UPS Protocol Debug Summary

## Device Information
- UPS Name: $UPS_NAME
- USB ID: $VENDOR_PRODUCT
- Date: $(date)

## Quick Status
$(upsc $UPS_NAME 2>/dev/null | grep -E "device.mfr|device.model|device.serial|ups.status" || echo "Could not retrieve device info")

## Files Collected
- lsusb.txt - USB device listing
- lsusb-verbose.txt - Detailed USB descriptor
- driver-test.txt - Initial driver connection test
- hid-paths.txt - All HID paths discovered
- upsc-full.txt - Complete UPS variables
- upscmd.txt - Supported commands
- online-reports.txt - HID reports during AC power
- battery-reports.txt - HID reports on battery
- charging-reports.txt - HID reports while charging
- *-upsc.txt - UPS variables for each state

## Next Steps
1. Review the collected data in $OUTPUT_DIR/
2. Create a new issue at: https://github.com/oleander/esphome-components/issues
3. Use the "New UPS Protocol Request" template
4. Attach or paste the relevant debug outputs

## Key Information for Issue Report
\`\`\`bash
# USB IDs
$VENDOR_PRODUCT

# Key Variables
$(upsc $UPS_NAME 2>/dev/null | grep -E "battery.charge|battery.runtime|ups.status|input.voltage" || echo "N/A")
\`\`\`
EOF

echo -e "\n${GREEN}=== Collection Complete ===${NC}"
echo ""
echo "Debug data saved to: $OUTPUT_DIR/"
echo "Summary available at: $OUTPUT_DIR/SUMMARY.md"
echo "Main output file: $OUTPUT_FILE"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Review the collected data"
echo "2. Create an issue at: https://github.com/oleander/esphome-components/issues"
echo "3. Use the 'New UPS Protocol Request' template"
echo "4. Paste relevant sections from $OUTPUT_FILE"
echo ""
echo -e "${GREEN}Thank you for helping improve UPS support!${NC}"

exit 0
