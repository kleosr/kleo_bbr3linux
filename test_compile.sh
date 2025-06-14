#!/bin/bash

# BBR3 Module Compilation Test Script
# This script tests if the BBR3 module can compile correctly

echo "====== BBR3 COMPILATION TEST ======"

# Check if we have the required files
if [ ! -f "tcp_bbr3.c" ]; then
    echo "ERROR: tcp_bbr3.c not found"
    exit 1
fi

if [ ! -f "Makefile" ]; then
    echo "ERROR: Makefile not found"
    exit 1
fi

echo "✓ Source files found"

# Check if we're on a Linux system with kernel headers
if [ ! -d "/lib/modules/$(uname -r)/build" ]; then
    echo "WARNING: This test requires Linux with kernel headers installed"
    echo "Current system: $(uname -s)"
    echo "This is a compilation test - the module is designed for Linux kernel"
    exit 0
fi

echo "✓ Kernel headers found at /lib/modules/$(uname -r)/build"

# Test compilation
echo "Testing module compilation..."
make clean 2>/dev/null || true

if make; then
    echo "✓ BBR3 module compiled successfully"
    
    if [ -f "tcp_bbr3.ko" ]; then
        echo "✓ Module file tcp_bbr3.ko created"
        
        # Check module info
        echo "Module information:"
        modinfo tcp_bbr3.ko 2>/dev/null || echo "Could not get module info"
        
        # Clean up
        make clean
        echo "✓ Cleaned up build files"
    else
        echo "✗ Module file tcp_bbr3.ko not found"
        exit 1
    fi
else
    echo "✗ Compilation failed"
    echo "Check the error messages above"
    exit 1
fi

echo "====== COMPILATION TEST PASSED ======" 