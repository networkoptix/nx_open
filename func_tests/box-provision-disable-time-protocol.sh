#!/bin/bash -xe

# Block time protocol ports
sudo iptables -A OUTPUT -p tcp --dport 37 -j DROP
sudo iptables -A OUTPUT -p tcp --dport 123 -j DROP
# Remove ntp service & utility
sudo apt-get remove -y ntp ntpdate
