#!/bin/bash -xe

# Block time protocol port and domain
sudo iptables -A OUTPUT -p tcp --dport 37 -j DROP
sudo iptables -A OUTPUT -p tcp --dport 123 -j DROP
sudo apt-get remove -y ntp ntpdate
