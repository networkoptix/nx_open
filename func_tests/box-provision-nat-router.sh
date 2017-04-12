#!/bin/bash -xe

echo 1 > /proc/sys/net/ipv4/ip_forward

iptables -t nat --flush
iptables --flush

iptables -t nat -A POSTROUTING -o eth1 -j MASQUERADE --random
iptables -A FORWARD -i eth1 -o eth2 -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
iptables -A FORWARD -i eth2 -o eth1 -j ACCEPT
iptables -A FORWARD -j DROP
