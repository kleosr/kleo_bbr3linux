# BBR3 Linux Optimization

This repository contains tools for optimizing TCP network performance on Linux systems using BBR congestion control.

## Contents

1. **BBR3 Kernel Module (`tcp_bbr3.c`)** - A custom kernel module implementing BBRv3 congestion control algorithm for advanced users who want to compile their own module.

2. **BBR Optimization Script (`bbr_optimized.sh`)** - A ready-to-use script that configures the built-in BBR congestion control with optimal settings for maximum network performance.

## BBR Optimization Script

This is the recommended approach for most users. The script optimizes the built-in BBR congestion control algorithm with settings that maximize network performance.

### Features

- Sets BBR as the default congestion control algorithm
- Configures the FQ packet scheduler for best performance with BBR
- Optimizes TCP buffer limits
- Applies advanced network optimizations 
- Configures the system for high-speed connections
- Makes all settings persistent across reboots

### Usage

```bash
sudo ./bbr_optimized.sh
```

## BBR3 Custom Kernel Module

For advanced users only. This custom kernel module implements BBRv3, an enhanced version of the BBR congestion control algorithm with additional optimizations.

### Features

- Three different modes: BBRv1, BBRv2, BBRv3
- Configurable parameters for fine-tuning
- DKMS support for automatic rebuilding when kernel updates

### Building and Installing

1. Install build dependencies:
   ```bash
   sudo apt install build-essential linux-headers-$(uname -r) dkms
   ```

2. Build with DKMS:
   ```bash
   sudo dkms add -m tcp_bbr3 -v 3.0
   sudo dkms build -m tcp_bbr3 -v 3.0
   sudo dkms install -m tcp_bbr3 -v 3.0
   ```

3. Load the module:
   ```bash
   sudo modprobe tcp_bbr3
   ```

4. Set as default congestion control:
   ```bash
   sudo sysctl -w net.ipv4.tcp_congestion_control=bbr3
   ```

## Which Approach to Use?

For most users, the **BBR Optimization Script** is recommended as it utilizes the stable and well-tested BBR implementation built into the Linux kernel with optimized settings.

The custom kernel module approach is for advanced users or developers who want to experiment with the latest BBR enhancements.

## License

This project is licensed under the GPL v2 License - see the LICENSE file for details.

## Acknowledgments

- Google for developing the BBR congestion control algorithm
- Linux kernel developers for implementing BBR in the mainline kernel 