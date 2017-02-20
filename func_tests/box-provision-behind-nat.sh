#!/bin/bash -xe

GATEWAY_ADDR=10.0.7.200

route add default gw $GATEWAY_ADDR

echo Disabling eth0
ip link set dev eth0 down
