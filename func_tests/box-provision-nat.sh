#!/bin/bash -xe

hostname
ifconfig

echo 1 > /proc/sys/net/ipv4/ip_forward
iptables --flush

iptables -t nat -A POSTROUTING -o eth1 -j MASQUERADE --random
iptables -A FORWARD -i eth1 -o eth2 -m state --state RELATED,ESTABLISHED -j ACCEPT
iptables -A FORWARD -i eth2 -o eth1 -j ACCEPT

ifconfig
