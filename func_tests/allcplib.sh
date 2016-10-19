#!/bin/bash
./vssh.sh 192.168.109.8 sudo cp /vagrant/$1 /opt/networkoptix/mediaserver/lib
./vssh.sh 192.168.109.9 sudo cp /vagrant/$1 /opt/networkoptix/mediaserver/lib

