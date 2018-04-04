"""Classes and functions unrelated to specific VM hypervisor (VirtualBox, libvirt, etc...)"""
from collections import namedtuple
from pprint import pformat

VMInfo = namedtuple('VMInfo', ['name', 'ports', 'macs', 'networks', 'is_running'])


class VMNotFound(Exception):
    pass


class VMAllAdaptersBusy(Exception):
    def __init__(self, vm_name, vm_networks):
        super(VMAllAdaptersBusy, self).__init__(
            "No available network adapter on {}:\n{}".format(vm_name, pformat(vm_networks)))
        self.vm_name = vm_name
        self.vm_networks = vm_networks
