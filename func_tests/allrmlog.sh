#!/bin/bash
./vssh.sh 192.168.109.8 sudo rm '/opt/networkoptix/mediaserver/var/log/*' 2> /dev/null
./vssh.sh 192.168.109.8 sudo rm /opt/networkoptix/mediaserver/bin/core 2> /dev/null
./vssh.sh 192.168.109.9 sudo rm '/opt/networkoptix/mediaserver/var/log/*' 2> /dev/null
./vssh.sh 192.168.109.9 sudo rm /opt/networkoptix/mediaserver/bin/core 2> /dev/null

