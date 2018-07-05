"""Classes and functions unrelated to specific VM hypervisor (VirtualBox, libvirt, etc...)"""
from abc import ABCMeta
from pprint import pformat


class Vm(object):
    __metaclass__ = ABCMeta

    def __init__(self, name, port_map, macs, networks, is_running):
        self.name = name
        self.port_map = port_map
        self.macs = macs
        self.networks = networks
        self.is_running = is_running

    def __repr__(self):
        if not self.is_running:
            return '<VM {!s} (stopped)>'.format(self.name)
        return '<VM {!s}>'.format(self.name)

    def find_vacant_nic(self):
        for key, occupation in self.networks.items():
            if occupation is None:
                return key
        raise VMAllAdaptersBusy(self.name, self.networks)


class VMNotFound(Exception):
    pass


class TemplateVMNotFound(Exception):
    pass


class VMAllAdaptersBusy(Exception):
    def __init__(self, vm_name, vm_networks):
        super(VMAllAdaptersBusy, self).__init__(
            "No available network adapter on {}:\n{}".format(vm_name, pformat(vm_networks)))
        self.vm_name = vm_name
        self.vm_networks = vm_networks
