#!/bin/bash
./vssh.sh Box1 sudo rm '/opt/networkoptix/mediaserver/var/log/*' 2> /dev/null
./vssh.sh Box1 sudo rm /opt/networkoptix/mediaserver/bin/core 2> /dev/null
./vssh.sh Box2 sudo rm '/opt/networkoptix/mediaserver/var/log/*' 2> /dev/null
./vssh.sh Box2 sudo rm /opt/networkoptix/mediaserver/bin/core 2> /dev/null

