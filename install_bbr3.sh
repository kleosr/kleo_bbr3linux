#!/bin/bash

# BBR3 Kernel Module Installation Script
# This script builds and installs the BBR3 congestion control module

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    print_error "Please run as root (with sudo)"
    exit 1
fi

# Get the directory where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "====== BBR3 KERNEL MODULE INSTALLER ======"
print_status "Installing BBR3 congestion control module..."

# Check for required files
REQUIRED_FILES=("tcp_bbr3.c" "Makefile" "dkms.conf")
for file in "${REQUIRED_FILES[@]}"; do
    if [ ! -f "$file" ]; then
        print_error "Required file $file not found in $SCRIPT_DIR"
        exit 1
    fi
done

# Check kernel version
KERNEL_VERSION=$(uname -r)
print_status "Current kernel version: $KERNEL_VERSION"

# Check if kernel headers are installed
KERNEL_HEADERS_DIR="/lib/modules/$KERNEL_VERSION/build"
if [ ! -d "$KERNEL_HEADERS_DIR" ]; then
    print_error "Kernel headers not found at $KERNEL_HEADERS_DIR"
    print_error "Please install kernel headers:"
    print_error "  Ubuntu/Debian: sudo apt install linux-headers-\$(uname -r)"
    print_error "  CentOS/RHEL:   sudo yum install kernel-devel"
    print_error "  Fedora:        sudo dnf install kernel-devel"
    exit 1
fi

# Check for build tools
print_step "Checking build dependencies..."
MISSING_TOOLS=()

if ! command -v make >/dev/null 2>&1; then
    MISSING_TOOLS+=("make")
fi

if ! command -v gcc >/dev/null 2>&1; then
    MISSING_TOOLS+=("gcc")
fi

if [ ${#MISSING_TOOLS[@]} -ne 0 ]; then
    print_error "Missing build tools: ${MISSING_TOOLS[*]}"
    print_error "Please install build essentials:"
    print_error "  Ubuntu/Debian: sudo apt install build-essential"
    print_error "  CentOS/RHEL:   sudo yum groupinstall 'Development Tools'"
    print_error "  Fedora:        sudo dnf groupinstall 'Development Tools'"
    exit 1
fi

# Check if DKMS is available and preferred
USE_DKMS=false
if command -v dkms >/dev/null 2>&1; then
    print_status "DKMS is available - using DKMS for installation"
    USE_DKMS=true
else
    print_warning "DKMS not found - using manual installation"
    print_warning "Install DKMS for automatic module rebuilding on kernel updates:"
    print_warning "  Ubuntu/Debian: sudo apt install dkms"
    print_warning "  CentOS/RHEL:   sudo yum install dkms"
    print_warning "  Fedora:        sudo dnf install dkms"
fi

# Function to install via DKMS
install_with_dkms() {
    print_step "Installing BBR3 module with DKMS..."
    
    # Remove existing DKMS module if present
    if dkms status | grep -q "tcp_bbr3"; then
        print_status "Removing existing BBR3 DKMS module..."
        dkms remove tcp_bbr3/3.0 --all 2>/dev/null || true
    fi
    
    # Create DKMS source directory
    DKMS_DIR="/usr/src/tcp_bbr3-3.0"
    print_status "Creating DKMS source directory: $DKMS_DIR"
    mkdir -p "$DKMS_DIR"
    
    # Copy source files
    cp tcp_bbr3.c Makefile dkms.conf "$DKMS_DIR/"
    
    # Add to DKMS
    print_status "Adding module to DKMS..."
    dkms add -m tcp_bbr3 -v 3.0
    
    # Build the module
    print_status "Building BBR3 module..."
    if ! dkms build -m tcp_bbr3 -v 3.0; then
        print_error "Failed to build BBR3 module with DKMS"
        return 1
    fi
    
    # Install the module
    print_status "Installing BBR3 module..."
    if ! dkms install -m tcp_bbr3 -v 3.0; then
        print_error "Failed to install BBR3 module with DKMS"
        return 1
    fi
    
    print_status "BBR3 module installed successfully with DKMS"
    return 0
}

# Function to install manually
install_manually() {
    print_step "Installing BBR3 module manually..."
    
    # Clean any previous builds
    print_status "Cleaning previous builds..."
    make clean 2>/dev/null || true
    
    # Build the module
    print_status "Building BBR3 module..."
    if ! make; then
        print_error "Failed to build BBR3 module"
        return 1
    fi
    
    # Check if module was built
    if [ ! -f "tcp_bbr3.ko" ]; then
        print_error "Module tcp_bbr3.ko was not created"
        return 1
    fi
    
    # Install the module
    print_status "Installing BBR3 module..."
    MODULE_DIR="/lib/modules/$KERNEL_VERSION/kernel/net/ipv4"
    mkdir -p "$MODULE_DIR"
    cp tcp_bbr3.ko "$MODULE_DIR/"
    
    # Update module dependencies
    print_status "Updating module dependencies..."
    depmod -a
    
    print_status "BBR3 module installed successfully"
    return 0
}

# Install the module
if [ "$USE_DKMS" = true ]; then
    if ! install_with_dkms; then
        print_warning "DKMS installation failed, trying manual installation..."
        install_manually
    fi
else
    install_manually
fi

# Load the module
print_step "Loading BBR3 module..."
if modprobe tcp_bbr3; then
    print_status "BBR3 module loaded successfully"
else
    print_error "Failed to load BBR3 module"
    print_error "Check dmesg for error messages: dmesg | tail -20"
    exit 1
fi

# Verify the module is loaded
if lsmod | grep -q tcp_bbr3; then
    print_status "✓ BBR3 module is loaded and active"
else
    print_error "✗ BBR3 module is not loaded"
    exit 1
fi

# Check if BBR3 is available as congestion control
if grep -q "bbr3" /proc/sys/net/ipv4/tcp_available_congestion_control 2>/dev/null; then
    print_status "✓ BBR3 is available as congestion control algorithm"
else
    print_warning "✗ BBR3 is not available as congestion control algorithm"
    print_warning "The module loaded but may not be properly registered"
fi

# Optionally set BBR3 as default
echo ""
read -p "Do you want to set BBR3 as the default congestion control algorithm? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    print_step "Setting BBR3 as default congestion control..."
    
    # Set for current session
    if sysctl -w net.ipv4.tcp_congestion_control=bbr3; then
        print_status "BBR3 set as current congestion control"
    else
        print_error "Failed to set BBR3 as congestion control"
        exit 1
    fi
    
    # Make it persistent
    echo "net.ipv4.tcp_congestion_control=bbr3" > /etc/sysctl.d/99-bbr3.conf
    print_status "BBR3 set as persistent default congestion control"
    
    # Verify
    CURRENT_CC=$(sysctl -n net.ipv4.tcp_congestion_control 2>/dev/null)
    if [ "$CURRENT_CC" = "bbr3" ]; then
        print_status "✓ BBR3 is now the active congestion control algorithm"
    else
        print_warning "✗ Current congestion control is still: $CURRENT_CC"
    fi
fi

echo ""
echo "====== INSTALLATION COMPLETE ======"
print_status "BBR3 kernel module has been successfully installed!"
print_status "Module info:"
modinfo tcp_bbr3 2>/dev/null | grep -E "(filename|version|description)" || print_warning "Could not get module info"

echo ""
print_status "Available congestion control algorithms:"
cat /proc/sys/net/ipv4/tcp_available_congestion_control

echo ""
print_status "Current congestion control: $(sysctl -n net.ipv4.tcp_congestion_control)"

echo ""
print_status "To unload the module: sudo modprobe -r tcp_bbr3"
if [ "$USE_DKMS" = true ]; then
    print_status "To remove DKMS module: sudo dkms remove tcp_bbr3/3.0 --all"
fi

echo "=====================================" 