#!/bin/bash
NAME=${1:?}-${2:-template}

if [[ $NAME = win* ]]; then
  VBoxManage modifyvm $NAME --cpus 2
  VBoxManage modifyvm $NAME --cpuexecutioncap 100
  VBoxManage modifyvm $NAME --memory 4096
  VBoxManage modifyvm $NAME --vram 256
  VBoxManage modifyvm $NAME --paravirtprovider hyperv
else
  VBoxManage modifyvm $NAME --cpus 1
  VBoxManage modifyvm $NAME --cpuexecutioncap 100
  VBoxManage modifyvm $NAME --memory 512
  VBoxManage modifyvm $NAME --vram 32
  VBoxManage modifyvm $NAME --paravirtprovider kvm
fi

VBoxManage modifyvm $NAME --nestedpaging on
VBoxManage modifyvm $NAME --hwvirtex on
VBoxManage modifyvm $NAME --ioapic on
VBoxManage modifyvm $NAME --audio none
VBoxManage setextradata $NAME VBoxInternal/Devices/VMMDev/0/Config/GetHostTimeDisabled 1

VBoxManage modifyvm $NAME --boot1 disk
VBoxManage modifyvm $NAME --boot2 none
VBoxManage modifyvm $NAME --boot3 none
VBoxManage modifyvm $NAME --boot4 none

VBoxManage modifyvm $NAME --nic1 nat
VBoxManage modifyvm $NAME --natnet1 192.168.254.0/24
VBoxManage modifyvm $NAME --nic2 null
VBoxManage modifyvm $NAME --nic3 null
VBoxManage modifyvm $NAME --nic4 null
VBoxManage modifyvm $NAME --nic5 null
VBoxManage modifyvm $NAME --nic6 null
VBoxManage modifyvm $NAME --nic7 null
VBoxManage modifyvm $NAME --nic8 null

VBoxManage snapshot $NAME delete template
VBoxManage snapshot $NAME take template
