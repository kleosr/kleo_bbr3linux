# BBR3 Linux Optimization - Fixed Implementation

This repository contains a **fixed and production-ready** implementation of BBR3 congestion control for Linux systems, along with optimization tools for maximum network performance.

## 🚀 What's New - Bug Fixes Applied

### Major Issues Fixed:
- ✅ **Kernel API Compatibility**: Fixed function signatures to match modern TCP congestion control API
- ✅ **Structure Size Optimization**: Reduced BBR3 structure size to fit within `ICSK_CA_PRIV_SIZE` limits
- ✅ **Missing Constants**: Added all required BBR constants and fallbacks for older kernels
- ✅ **Rate Sampling**: Implemented proper bandwidth estimation and RTT tracking
- ✅ **Memory Management**: Fixed initialization and proper use of `inet_csk_ca()`
- ✅ **State Machine**: Implemented proper BBR state transitions
- ✅ **Error Handling**: Added comprehensive error checking and validation
- ✅ **Build System**: Fixed Makefile and DKMS configuration

## 📁 Contents

1. **BBR3 Kernel Module (`tcp_bbr3.c`)** - Fixed implementation of BBRv3 congestion control
2. **BBR Optimization Script (`bbr_optimized.sh`)** - Enhanced script with error handling and validation
3. **Installation Script (`install_bbr3.sh`)** - Automated installation with DKMS support
4. **Test Script (`test_compile.sh`)** - Compilation validation tool

## 🎯 Quick Start - Recommended Approach

### Option 1: Use Built-in BBR with Optimizations (Recommended)
For most users, this is the safest and most effective approach:

```bash
sudo ./bbr_optimized.sh
```

This script:
- ✅ Validates BBR availability in your kernel
- ✅ Configures optimal BBR settings
- ✅ Sets up FQ packet scheduler
- ✅ Optimizes TCP buffers and parameters
- ✅ Makes settings persistent across reboots
- ✅ Creates automatic backups

### Option 2: Install Custom BBR3 Module (Advanced Users)
For users who want the latest BBR3 enhancements:

```bash
sudo ./install_bbr3.sh
```

## 🔧 Manual Installation

### Prerequisites

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r) dkms
```

**CentOS/RHEL:**
```bash
sudo yum groupinstall "Development Tools"
sudo yum install kernel-devel dkms
```

**Fedora:**
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install kernel-devel dkms
```

### BBR3 Module Installation

1. **Clone and enter directory:**
   ```bash
   git clone <repository-url>
   cd kleo_bbr3linux
   ```

2. **Test compilation (optional):**
   ```bash
   ./test_compile.sh
   ```

3. **Install with DKMS (recommended):**
   ```bash
   sudo dkms add -m tcp_bbr3 -v 3.0
   sudo dkms build -m tcp_bbr3 -v 3.0
   sudo dkms install -m tcp_bbr3 -v 3.0
   ```

4. **Or install manually:**
   ```bash
   make
   sudo make install
   sudo depmod -a
   ```

5. **Load the module:**
   ```bash
   sudo modprobe tcp_bbr3
   ```

6. **Set as default congestion control:**
   ```bash
   sudo sysctl -w net.ipv4.tcp_congestion_control=bbr3
   echo "net.ipv4.tcp_congestion_control=bbr3" | sudo tee /etc/sysctl.d/99-bbr3.conf
   ```

## 🔍 Verification

### Check if BBR3 is loaded:
```bash
lsmod | grep tcp_bbr3
```

### Check available congestion control algorithms:
```bash
cat /proc/sys/net/ipv4/tcp_available_congestion_control
```

### Check current congestion control:
```bash
sysctl net.ipv4.tcp_congestion_control
```

### View module information:
```bash
modinfo tcp_bbr3
```

## �� BBR3 Features

### Module Parameters
- `bbr_mode`: BBR version (0=BBRv1, 1=BBRv2, 2=BBRv3) - Default: 2
- `fast_convergence`: Enable fast convergence - Default: 1
- `drain_to_target`: Enable drain to target - Default: 1
- `min_rtt_win_sec`: Min RTT filter window length (sec) - Default: 5

### Key Improvements Over Standard BBR
- 🚀 **Enhanced Bandwidth Estimation**: More accurate bandwidth detection
- 📈 **Improved State Machine**: Better handling of network conditions
- 🎯 **Optimized Congestion Window**: More efficient window management
- 🔄 **Better Loss Recovery**: Improved handling of packet loss
- ⚡ **Lower Latency**: Reduced bufferbloat and improved responsiveness

## 🛠️ Troubleshooting

### Module Won't Load
```bash
# Check kernel logs
dmesg | tail -20

# Verify kernel version (requires 4.9+)
uname -r

# Check if headers are installed
ls /lib/modules/$(uname -r)/build
```

### Compilation Errors
```bash
# Install missing dependencies
sudo apt install build-essential linux-headers-$(uname -r)

# Clean and rebuild
make clean
make
```

### BBR3 Not Available
```bash
# Check if module is loaded
lsmod | grep tcp_bbr3

# Check registration
grep bbr3 /proc/sys/net/ipv4/tcp_available_congestion_control
```

### Performance Issues
```bash
# Check current settings
sysctl -a | grep -E "(tcp_congestion|default_qdisc)"

# Monitor connections
ss -i | grep bbr3

# Check network statistics
cat /proc/net/netstat | grep -E "(TcpExt|IpExt)"
```

## 🔄 Uninstallation

### Remove BBR3 Module
```bash
# Unload module
sudo modprobe -r tcp_bbr3

# Remove DKMS module
sudo dkms remove tcp_bbr3/3.0 --all

# Remove configuration
sudo rm -f /etc/sysctl.d/99-bbr3.conf
```

### Restore Original Settings
```bash
# Use backup created by bbr_optimized.sh
sudo sysctl -p /tmp/network_settings_backup_*.txt

# Or manually reset
sudo sysctl -w net.ipv4.tcp_congestion_control=cubic
sudo sysctl -w net.core.default_qdisc=pfifo_fast
```

## 📈 Performance Testing

### Basic Speed Test
```bash
# Install iperf3
sudo apt install iperf3

# Test with BBR3
iperf3 -c <server> -t 60 -i 5

# Compare with other algorithms
sudo sysctl -w net.ipv4.tcp_congestion_control=cubic
iperf3 -c <server> -t 60 -i 5
```

### Monitor Real-time Performance
```bash
# Watch congestion control in action
watch -n 1 'ss -i | grep -E "(bbr3|State)"'

# Monitor system network stats
watch -n 1 'cat /proc/net/netstat | grep TcpExt'
```

## ⚠️ Important Notes

- **Kernel Compatibility**: Requires Linux kernel 4.9 or later
- **Root Access**: Installation requires root privileges
- **Network Impact**: Changes affect all TCP connections system-wide
- **Testing Recommended**: Test in non-production environments first
- **Backup**: Always backup current settings before applying changes

## 🤝 Contributing

Found a bug or want to improve the implementation? Please:
1. Test your changes thoroughly
2. Ensure compatibility across different kernel versions
3. Add appropriate error handling
4. Update documentation

## 📄 License

This project is licensed under the GPL v2 License - see the LICENSE file for details.

## 🙏 Acknowledgments

- Google for developing the BBR congestion control algorithm
- Linux kernel developers for implementing BBR in the mainline kernel
- The networking community for continuous improvements and feedback

---

**Note**: This implementation has been thoroughly tested and debugged to resolve common kernel module compilation and runtime issues. The fixes ensure compatibility with modern Linux kernels while maintaining the performance benefits of BBR3. 