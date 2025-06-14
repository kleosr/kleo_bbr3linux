#!/bin/bash

# BBR Optimization Script for Maximum Performance
# This configures the built-in BBR with optimized settings

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
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

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    print_error "Please run as root (with sudo)"
    exit 1
fi

# Check if BBR is available in the kernel
if ! grep -q "bbr" /proc/sys/net/ipv4/tcp_available_congestion_control 2>/dev/null; then
    print_error "BBR congestion control is not available in this kernel"
    print_error "Please ensure your kernel version is 4.9 or later"
    exit 1
fi

# Check if FQ qdisc is available
if ! tc qdisc help 2>&1 | grep -q "fq"; then
    print_warning "FQ qdisc may not be available, falling back to fq_codel"
    DEFAULT_QDISC="fq_codel"
else
    DEFAULT_QDISC="fq"
fi

echo "====== BBR OPTIMIZATION SCRIPT ======"
print_status "Configuring BBR for maximum performance..."

# Backup current settings
BACKUP_FILE="/tmp/network_settings_backup_$(date +%Y%m%d_%H%M%S).txt"
print_status "Creating backup of current settings to $BACKUP_FILE"
{
    echo "# Network settings backup - $(date)"
    echo "net.ipv4.tcp_congestion_control=$(sysctl -n net.ipv4.tcp_congestion_control 2>/dev/null || echo 'unknown')"
    echo "net.core.default_qdisc=$(sysctl -n net.core.default_qdisc 2>/dev/null || echo 'unknown')"
    echo "net.core.rmem_max=$(sysctl -n net.core.rmem_max 2>/dev/null || echo 'unknown')"
    echo "net.core.wmem_max=$(sysctl -n net.core.wmem_max 2>/dev/null || echo 'unknown')"
} > "$BACKUP_FILE"

# Enable BBR congestion control algorithm
print_status "Setting BBR as the default congestion control algorithm"
if ! sysctl -w net.ipv4.tcp_congestion_control=bbr; then
    print_error "Failed to set BBR as congestion control"
    exit 1
fi

# Enable FQ packet scheduler (best pairing with BBR)
print_status "Setting $DEFAULT_QDISC packet scheduler"
if ! sysctl -w net.core.default_qdisc="$DEFAULT_QDISC"; then
    print_warning "Failed to set $DEFAULT_QDISC, trying fq_codel"
    sysctl -w net.core.default_qdisc=fq_codel || print_warning "Failed to set any qdisc"
fi

# Increase TCP buffer limits
print_status "Optimizing TCP buffer limits"
sysctl -w net.core.rmem_max=26214400 || print_warning "Failed to set rmem_max"
sysctl -w net.core.wmem_max=26214400 || print_warning "Failed to set wmem_max"
sysctl -w net.core.rmem_default=212992 || print_warning "Failed to set rmem_default"
sysctl -w net.core.wmem_default=212992 || print_warning "Failed to set wmem_default"
sysctl -w net.ipv4.tcp_rmem="4096 87380 16777216" || print_warning "Failed to set tcp_rmem"
sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216" || print_warning "Failed to set tcp_wmem"
sysctl -w net.ipv4.tcp_mem="16777216 16777216 16777216" || print_warning "Failed to set tcp_mem"

# Advanced BBR-compatible optimizations
print_status "Applying advanced network optimizations"
sysctl -w net.ipv4.tcp_fastopen=3 || print_warning "Failed to set tcp_fastopen"
sysctl -w net.ipv4.tcp_slow_start_after_idle=0 || print_warning "Failed to set tcp_slow_start_after_idle"
sysctl -w net.ipv4.tcp_mtu_probing=1 || print_warning "Failed to set tcp_mtu_probing"
sysctl -w net.ipv4.tcp_timestamps=1 || print_warning "Failed to set tcp_timestamps"
sysctl -w net.ipv4.tcp_sack=1 || print_warning "Failed to set tcp_sack"
sysctl -w net.core.netdev_max_backlog=5000 || print_warning "Failed to set netdev_max_backlog"

# Optimize for high-speed, high-latency networks
print_status "Optimizing for high-speed connections"
sysctl -w net.ipv4.tcp_window_scaling=1 || print_warning "Failed to set tcp_window_scaling"
sysctl -w net.ipv4.tcp_adv_win_scale=2 || print_warning "Failed to set tcp_adv_win_scale"
sysctl -w net.ipv4.tcp_moderate_rcvbuf=1 || print_warning "Failed to set tcp_moderate_rcvbuf"

# Additional optimizations for BBR
print_status "Applying additional BBR optimizations"
sysctl -w net.ipv4.tcp_notsent_lowat=16384 || print_warning "Failed to set tcp_notsent_lowat"
sysctl -w net.ipv4.tcp_low_latency=1 || print_warning "Failed to set tcp_low_latency (may not be available)"

# Make settings persistent
print_status "Making settings persistent"
SYSCTL_CONF="/etc/sysctl.d/99-bbr-optimized.conf"

# Create backup of existing config if it exists
if [ -f "$SYSCTL_CONF" ]; then
    cp "$SYSCTL_CONF" "${SYSCTL_CONF}.backup.$(date +%Y%m%d_%H%M%S)"
fi

cat > "$SYSCTL_CONF" <<EOF
# BBR congestion control with optimized settings
# Generated on $(date)
# Backup saved to: $BACKUP_FILE

# Core BBR settings
net.ipv4.tcp_congestion_control=bbr
net.core.default_qdisc=$DEFAULT_QDISC

# Buffer optimizations
net.core.rmem_max=26214400
net.core.wmem_max=26214400
net.core.rmem_default=212992
net.core.wmem_default=212992
net.ipv4.tcp_rmem=4096 87380 16777216
net.ipv4.tcp_wmem=4096 65536 16777216
net.ipv4.tcp_mem=16777216 16777216 16777216

# Advanced TCP optimizations
net.ipv4.tcp_fastopen=3
net.ipv4.tcp_slow_start_after_idle=0
net.ipv4.tcp_mtu_probing=1
net.ipv4.tcp_timestamps=1
net.ipv4.tcp_sack=1
net.core.netdev_max_backlog=5000

# High-speed network optimizations
net.ipv4.tcp_window_scaling=1
net.ipv4.tcp_adv_win_scale=2
net.ipv4.tcp_moderate_rcvbuf=1

# Additional BBR optimizations
net.ipv4.tcp_notsent_lowat=16384
EOF

# Apply the persistent settings
print_status "Applying persistent settings"
if sysctl -p "$SYSCTL_CONF"; then
    print_status "Persistent settings applied successfully"
else
    print_warning "Some persistent settings failed to apply"
fi

# Verify BBR is active
CURRENT_CC=$(sysctl -n net.ipv4.tcp_congestion_control 2>/dev/null)
CURRENT_QDISC=$(sysctl -n net.core.default_qdisc 2>/dev/null)

echo "===================================="
print_status "BBR optimization complete!"

if [ "$CURRENT_CC" = "bbr" ]; then
    print_status "✓ BBR congestion control is active"
else
    print_warning "✗ BBR congestion control is NOT active (current: $CURRENT_CC)"
fi

if [ "$CURRENT_QDISC" = "$DEFAULT_QDISC" ]; then
    print_status "✓ $DEFAULT_QDISC packet scheduler is active"
else
    print_warning "✗ $DEFAULT_QDISC packet scheduler is NOT active (current: $CURRENT_QDISC)"
fi

print_status "Your network performance should now be significantly improved."
print_status "Settings backup saved to: $BACKUP_FILE"
print_status "Configuration saved to: $SYSCTL_CONF"
echo "===================================="

# Optional: Show some network statistics
if command -v ss >/dev/null 2>&1; then
    echo ""
    print_status "Current TCP connections summary:"
    ss -s | grep -E "(TCP|Total)"
fi 