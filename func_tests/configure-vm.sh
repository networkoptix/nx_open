#!/bin/bash

set -eux

NAME=${1:?"Usage: ./configure-vm.sh <vm_name>"}

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
VBoxManage modifyvm $NAME --nic2 nat
VBoxManage modifyvm $NAME --natnet2 192.168.253.0/24
VBoxManage modifyvm $NAME --nic3 null
VBoxManage modifyvm $NAME --nic4 null
VBoxManage modifyvm $NAME --nic5 null
VBoxManage modifyvm $NAME --nic6 null
VBoxManage modifyvm $NAME --nic7 null
VBoxManage modifyvm $NAME --nic8 null
VBoxManage modifyvm $NAME --nictype1 virtio
VBoxManage modifyvm $NAME --nictype2 virtio
VBoxManage modifyvm $NAME --nictype3 virtio
VBoxManage modifyvm $NAME --nictype4 virtio
VBoxManage modifyvm $NAME --nictype5 virtio
VBoxManage modifyvm $NAME --nictype6 virtio
VBoxManage modifyvm $NAME --nictype7 virtio
VBoxManage modifyvm $NAME --nictype8 virtio
VBoxManage modifyvm $NAME --nicbandwidthgroup1 none
VBoxManage modifyvm $NAME --nicbandwidthgroup2 none
VBoxManage modifyvm $NAME --nicbandwidthgroup3 none
VBoxManage modifyvm $NAME --nicbandwidthgroup4 none
VBoxManage modifyvm $NAME --nicbandwidthgroup5 none
VBoxManage modifyvm $NAME --nicbandwidthgroup6 none
VBoxManage modifyvm $NAME --nicbandwidthgroup7 none
VBoxManage modifyvm $NAME --nicbandwidthgroup8 none
VBoxManage bandwidthctl $NAME list --machinereadable | while read line; do
    value=${line#*=}
    group=${value%%,*}
    VBoxManage bandwidthctl $NAME remove $group
done
VBoxManage bandwidthctl $NAME add network1 --type network --limit 0
VBoxManage bandwidthctl $NAME add network2 --type network --limit 0
VBoxManage bandwidthctl $NAME add network3 --type network --limit 0
VBoxManage bandwidthctl $NAME add network4 --type network --limit 0
VBoxManage bandwidthctl $NAME add network5 --type network --limit 0
VBoxManage bandwidthctl $NAME add network6 --type network --limit 0
VBoxManage bandwidthctl $NAME add network7 --type network --limit 0
VBoxManage bandwidthctl $NAME add network8 --type network --limit 0
VBoxManage modifyvm $NAME --nicbandwidthgroup1 network1
VBoxManage modifyvm $NAME --nicbandwidthgroup2 network2
VBoxManage modifyvm $NAME --nicbandwidthgroup3 network3
VBoxManage modifyvm $NAME --nicbandwidthgroup4 network4
VBoxManage modifyvm $NAME --nicbandwidthgroup5 network5
VBoxManage modifyvm $NAME --nicbandwidthgroup6 network6
VBoxManage modifyvm $NAME --nicbandwidthgroup7 network7
VBoxManage modifyvm $NAME --nicbandwidthgroup8 network8
