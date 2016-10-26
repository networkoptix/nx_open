#!/bin/bash
./vssh.sh Box1 sudo cp /vagrant/$1 /opt/networkoptix/mediaserver/lib
./vssh.sh Box2 sudo cp /vagrant/$1 /opt/networkoptix/mediaserver/lib

