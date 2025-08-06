# Development Tools

This folder contains development tools for the NUT UPS ESPHome component.

## scan-usb.sh

This script lists esp and ups releated usb devices.

## generate_vendor_list.py

This script keeps the UPS vendor ID lists synchronized between C++, scripts and Python code.

### Purpose

The NUT UPS component maintains a list of known UPS vendor IDs in two places:
- `components/nut_ups/ups_vendors.h` (C++ runtime code)
- `components/nut_ups/__init__.py` (Python configuration validation)

This script extracts the vendor list from the C++ header file and updates the Python file to ensure they stay in sync.

### Usage

#### Command Line
```bash
cd /workspace
python3 tools/generate_vendor_list.py
```

#### VSCode Task
1. Open the Command Palette (`Ctrl+Shift+P` or `Cmd+Shift+P`)
2. Type "Tasks: Run Task"
3. Select "Generate UPS Vendor List"

Or use the keyboard shortcut `Ctrl+Shift+P` -> "Tasks: Run Build Task" and select the vendor list generator.

### When to Run

Run this script whenever you:
1. Add new UPS vendor IDs to `components/nut_ups/ups_vendors.h`
2. Modify existing vendor information
3. Want to ensure the C++ and Python lists are synchronized

### Output

The script will:
- Parse the C++ header file for vendor definitions
- Extract vendor IDs, names, and descriptions
- Update the `KNOWN_VENDOR_IDS` dictionary in `__init__.py`
- Display the number of vendors processed

### Example Output

```
Parsing vendors from /workspace/components/nut_ups/ups_vendors.h...
Found 7 vendors
Updated /workspace/components/nut_ups/__init__.py
```

## Adding New Vendors

To add a new UPS vendor:

1. Edit `components/nut_ups/ups_vendors.h`
2. Add a new entry to the `KNOWN_UPS_VENDORS` array:
   ```cpp
   {0x1234, "New Vendor", "New Vendor Description"},
   ```
3. Run the generate_vendor_list.py script to update the Python file
4. Compile and test the component

The script automatically maintains alphabetical ordering by vendor ID.