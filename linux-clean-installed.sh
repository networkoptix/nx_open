#!/bin/sh

sudo dpkg -r networkoptix-entcontroller
sudo dpkg --purge networkoptix-entcontroller
sudo dpkg -r networkoptix-mediaserver
sudo dpkg -r networkoptix-client
sudo rm -rf /opt/networkoptix
sudo rm -rf ~/.config/Network\ Optix/
sudo rm -rf ~/.local/share/data/Network\ Optix/

sudo dpkg -r digitalwatchdog-entcontroller
sudo dpkg --purge digitalwatchdog-entcontroller
sudo dpkg -r digitalwatchdog-mediaserver
sudo dpkg -r digitalwatchdog-client
sudo rm -rf /opt/digitalwatchdog
sudo rm -rf ~/.config/Digital\ Watchdog/
sudo rm -rf ~/.local/share/data/Digital\ Watchdog/

sudo dpkg -r nnodal-entcontroller
sudo dpkg --purge nnodal-entcontroller
sudo dpkg -r nnodal-mediaserver
sudo dpkg -r nnodal-client
sudo rm -rf /opt/nnodal
sudo rm -rf ~/.config/nnodal
sudo rm -rf ~/.local/share/data/nnodal