#!/usr/bin/env python3
"""
Script to extract UPS vendor IDs from ups_vendors.h and generate Python dict.
This ensures the Python configuration validation uses the same vendor list as C++.
"""

import re
from pathlib import Path


def parse_vendor_header(header_path):
    """Parse ups_vendors.h and extract vendor information."""
    vendors = {}

    with open(header_path, "r") as f:
        content = f.read()

    # Find the KNOWN_UPS_VENDORS array
    pattern = r'\{0x([0-9A-Fa-f]+),\s*"([^"]+)",\s*"([^"]+)"\}'
    matches = re.findall(pattern, content)

    for match in matches:
        vendor_id_str, name, description = match
        vendor_id = int(vendor_id_str, 16)
        vendors[vendor_id] = {"name": name, "description": description}

    return vendors


def generate_python_dict(vendors):
    """Generate Python dictionary code."""
    lines = [
        "# Known UPS vendor IDs for validation",
        "# Auto-generated from ups_vendors.h",
        "KNOWN_VENDOR_IDS = {",
    ]

    for vendor_id, info in sorted(vendors.items()):
        lines.append(f'    0x{vendor_id:04X}: "{info["name"]}",')

    lines.append("}")
    return "\n".join(lines)


def generate_udev_rules(vendors):
    """Generate udev rules for UPS vendors."""
    lines = []

    # Standard ESP development boards (keep existing ones)
    standard_vendors = [
        ("303a", "Espressif ESP32-S2/S3"),
        ("10c4", "Silicon Labs CP210x"),
        ("1a86", "QinHeng Electronics CH340"),
        ("0403", "FTDI USB Serial"),
    ]

    for vid, desc in standard_vendors:
        lines.append(f'SUBSYSTEM=="usb", ATTR{{idVendor}}=="{vid}", MODE="0666"')

    # UPS vendors from header file
    for vendor_id, info in sorted(vendors.items()):
        vid_hex = f"{vendor_id:04x}"  # lowercase for udev
        lines.append(f'SUBSYSTEM=="usb", ATTR{{idVendor}}=="{vid_hex}", MODE="0666"')

    return lines


def update_udev_rules_in_setup_sh(setup_path, udev_rules):
    """Update udev rules in setup.sh file."""
    if not setup_path.exists():
        print(f"Warning: {setup_path} not found!")
        return False

    with open(setup_path, "r") as f:
        content = f.read()

    # Find the udev rules section
    udev_start = "cat > /etc/udev/rules.d/99-esphome-usb.rules << 'UDEV_EOF'"
    udev_end = "UDEV_EOF"

    start_pos = content.find(udev_start)
    if start_pos == -1:
        print(f"Warning: Could not find udev rules section in {setup_path}")
        return False

    end_pos = content.find(udev_end, start_pos + len(udev_start))
    if end_pos == -1:
        print(f"Warning: Could not find end of udev rules section in {setup_path}")
        return False

    # Build the new udev section
    new_udev_section = udev_start + "\n"
    for rule in udev_rules:
        new_udev_section += rule + "\n"
    new_udev_section += udev_end

    # Replace the section
    new_content = (
        content[:start_pos] + new_udev_section + content[end_pos + len(udev_end) :]
    )

    with open(setup_path, "w") as f:
        f.write(new_content)

    # @TODO: reload udev if possible

    return True


def update_scan_usb_vendors(scan_usb_path, vendors):
    """Update vendor list in scan-usb.sh file."""
    if not scan_usb_path.exists():
        print(f"Warning: {scan_usb_path} not found!")
        return False

    with open(scan_usb_path, "r") as f:
        content = f.read()

    # Generate the vendor ID pattern for grep
    vendor_ids = [f"{vid:04x}" for vid in sorted(vendors.keys())]
    vendor_pattern = "|".join(vendor_ids)

    # Find and replace the UPS devices grep line
    new_line = f'lsusb | grep -iE "({vendor_pattern})"'
    
    # Replace the grep line in the UPS devices section
    import re
    ups_section_pattern = r'(# Auto-generated from ups_vendors\.h\necho "UPS devices:"\n)lsusb \| grep -iE "\([^"]*\)"'
    replacement = rf'\1{new_line}'
    
    new_content = re.sub(ups_section_pattern, replacement, content)
    
    if new_content != content:
        with open(scan_usb_path, "w") as f:
            f.write(new_content)
        return True
    else:
        print(f"Warning: Could not find UPS devices section in {scan_usb_path}")
        return False


def main():
    """Main function to update __init__.py, setup.sh, and scan-usb.sh with current vendor list."""
    script_dir = Path(__file__).parent
    component_dir = script_dir.parent / "components" / "nut_ups"
    header_path = component_dir / "ups_vendors.h"
    init_path = component_dir / "__init__.py"
    setup_path = script_dir.parent / ".devcontainer" / "setup.sh"
    scan_usb_path = script_dir / "scan-usb.sh"

    if not header_path.exists():
        print(f"Error: {header_path} not found!")
        return 1

    print(f"Parsing vendors from {header_path}...")
    vendors = parse_vendor_header(header_path)
    print(f"Found {len(vendors)} UPS vendors")

    # Generate the Python code
    python_code = generate_python_dict(vendors)

    # Update Python __init__.py
    if init_path.exists():
        # Read current __init__.py
        with open(init_path, "r") as f:
            content = f.read()

        # Replace the KNOWN_VENDOR_IDS section
        pattern = r"KNOWN_VENDOR_IDS = \{[^}]*\}"
        if re.search(pattern, content, re.DOTALL):
            new_content = re.sub(
                pattern, python_code.split("\n", 2)[2], content, flags=re.DOTALL
            )

            with open(init_path, "w") as f:
                f.write(new_content)
            print(f"✅ Updated {init_path}")
        else:
            print(f"Warning: Could not find KNOWN_VENDOR_IDS in {init_path}")
            print("Generated Python code:")
            print(python_code)
    else:
        print("Generated Python code:")
        print(python_code)

    # Generate and update udev rules
    udev_rules = generate_udev_rules(vendors)
    if update_udev_rules_in_setup_sh(setup_path, udev_rules):
        print(f"✅ Updated udev rules in {setup_path}")
        print(
            f"   Added {len([r for r in udev_rules if 'UPS' not in r])} standard + {len(vendors)} UPS vendor rules"
        )

    # Update scan-usb.sh vendor list
    if update_scan_usb_vendors(scan_usb_path, vendors):
        print(f"✅ Updated vendor list in {scan_usb_path}")
        print(f"   UPS vendor pattern: ({len(vendors)} vendors)")

    return 0


if __name__ == "__main__":
    exit(main())
