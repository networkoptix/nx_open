"""Classes and functions unrelated to specific VM hypervisor (VirtualBox, libvirt, etc...)"""
from collections import namedtuple
from pprint import pformat

VMInfo = namedtuple('VMInfo', ['name', 'ports', 'macs', 'networks', 'is_running'])  # TODO: Rename to VMHardware.


class VMNotFound(Exception):
    pass


class VMAllAdaptersBusy(Exception):
    def __init__(self, vm_name, vm_networks):
        super(VMAllAdaptersBusy, self).__init__(
            "No available network adapter on {}:\n{}".format(vm_name, pformat(vm_networks)))
        self.vm_name = vm_name
        self.vm_networks = vm_networks


def obtain_running_vm(hypervisor, vm_name, vm_index, vm_configuration):
    try:
        vm_info = hypervisor.find(vm_name)
    except VMNotFound:
        vm_info = hypervisor.clone(vm_name, vm_index, vm_configuration)
    assert vm_info.name == vm_name
    if not vm_info.is_running:
        hypervisor.power_on(vm_info.name)
    return vm_info

