#!/bin/bash

# BBR Optimization Script for Maximum Performance
# This configures the built-in BBR with optimized settings

# Check if running as root
if [ "$EUID" -ne 0 ]; then
  echo "Please run as root (with sudo)"
  exit 1
fi

echo "====== BBR OPTIMIZATION SCRIPT ======"
echo "Configuring BBR for maximum performance..."

# Enable BBR congestion control algorithm
echo "Setting BBR as the default congestion control algorithm"
sysctl -w net.ipv4.tcp_congestion_control=bbr

# Enable FQ packet scheduler (best pairing with BBR)
echo "Setting FQ packet scheduler"
sysctl -w net.core.default_qdisc=fq

# Increase TCP buffer limits
echo "Optimizing TCP buffer limits"
sysctl -w net.core.rmem_max=26214400
sysctl -w net.core.wmem_max=26214400
sysctl -w net.core.rmem_default=212992
sysctl -w net.core.wmem_default=212992
sysctl -w net.ipv4.tcp_rmem="4096 87380 16777216"
sysctl -w net.ipv4.tcp_wmem="4096 65536 16777216"
sysctl -w net.ipv4.tcp_mem="16777216 16777216 16777216"

# Advanced BBR-compatible optimizations
echo "Applying advanced network optimizations"
sysctl -w net.ipv4.tcp_fastopen=3
sysctl -w net.ipv4.tcp_slow_start_after_idle=0
sysctl -w net.ipv4.tcp_mtu_probing=1
sysctl -w net.ipv4.tcp_timestamps=1
sysctl -w net.ipv4.tcp_sack=1
sysctl -w net.core.netdev_max_backlog=5000

# Optimize for high-speed, high-latency networks
echo "Optimizing for high-speed connections"
sysctl -w net.ipv4.tcp_window_scaling=1
sysctl -w net.ipv4.tcp_adv_win_scale=2
sysctl -w net.ipv4.tcp_moderate_rcvbuf=1

# Make settings persistent
echo "Making settings persistent"
cat > /etc/sysctl.d/99-bbr-optimized.conf <<EOF
# BBR congestion control with optimized settings
net.ipv4.tcp_congestion_control=bbr
net.core.default_qdisc=fq
net.core.rmem_max=26214400
net.core.wmem_max=26214400
net.core.rmem_default=212992
net.core.wmem_default=212992
net.ipv4.tcp_rmem=4096 87380 16777216
net.ipv4.tcp_wmem=4096 65536 16777216
net.ipv4.tcp_mem=16777216 16777216 16777216
net.ipv4.tcp_fastopen=3
net.ipv4.tcp_slow_start_after_idle=0
net.ipv4.tcp_mtu_probing=1
net.ipv4.tcp_timestamps=1
net.ipv4.tcp_sack=1
net.core.netdev_max_backlog=5000
net.ipv4.tcp_window_scaling=1
net.ipv4.tcp_adv_win_scale=2
net.ipv4.tcp_moderate_rcvbuf=1
EOF

# Apply the persistent settings
sysctl -p /etc/sysctl.d/99-bbr-optimized.conf

echo "===================================="
echo "BBR optimization complete!"
echo "Your network performance should now be significantly improved."
echo "Current congestion control: $(sysctl -n net.ipv4.tcp_congestion_control)"
echo "====================================" 