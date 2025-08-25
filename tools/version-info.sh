#!/bin/bash

# Script to generate version string with git hash
# Usage: ./version.sh

set -euo pipefail  # Exit on error, undefined vars, pipe failures

# Configuration
readonly VERSION_BASE="0.0.0"
readonly SCRIPT_NAME="$(basename "$0")"

# Function to get git hash
get_git_hash() {
    local git_hash
    
    # Check if we're in a git repository
    if ! git rev-parse --git-dir >/dev/null 2>&1; then
        echo "Error: Not in a git repository" >&2
        return 1
    fi
    
    # Get short git hash (7 characters)
    if ! git_hash=$(git rev-parse --short HEAD 2>/dev/null); then
        echo "Error: Failed to get git hash" >&2
        return 1
    fi
    
    echo "$git_hash"
}

# Function to generate version string
generate_version() {
    local git_hash
    
    if git_hash=$(get_git_hash); then
        echo "${VERSION_BASE}-${git_hash}"
    else
        return 1
    fi
}

# Function to display usage
usage() {
    cat << EOF
Usage: $SCRIPT_NAME [OPTIONS]

Generate a version string in format: $VERSION_BASE-<git_hash>

OPTIONS:
    -h, --help    Show this help message

EXAMPLES:
    $SCRIPT_NAME           # Output: 0.0.0-a1b2c3d
EOF
}

# Main function
main() {
    # Parse command line arguments
    case "${1:-}" in
        -h|--help)
            usage
            exit 0
            ;;
        "")
            # No arguments - proceed with version generation
            ;;
        *)
            echo "Error: Unknown option '$1'" >&2
            usage >&2
            exit 1
            ;;
    esac
    
    # Generate and output version string
    if ! generate_version; then
        exit 1
    fi
}

# Only run main if script is executed directly (not sourced)
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi