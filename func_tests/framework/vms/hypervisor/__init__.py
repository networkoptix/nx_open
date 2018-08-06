"""Classes and functions unrelated to specific VM hypervisor (VirtualBox, libvirt, etc...)"""
from abc import ABCMeta, abstractmethod
from pprint import pformat


# TODO: Merge `VmHardware` and `VM` namedtuple; this awkward name should go away.
class VmHardware(object):
    """Settings hypervisor is responsible for"""
    __metaclass__ = ABCMeta

    def __init__(self, name, port_map, macs, vacant_nics, description):
        self.name = name
        self.port_map = port_map
        self.macs = macs
        self._vacant_nics = list(reversed(vacant_nics))
        self.description = description

    def __repr__(self):
        return '<VM {!s}>'.format(self.name)

    def _find_vacant_nic(self):
        return self._vacant_nics.pop()

    @abstractmethod
    def clone(self, clone_vm_name):  # type: (str) -> VmHardware
        pass

    @abstractmethod
    def export(self, vm_image_path):
        """Export VM from its current state: it may not have snapshot at all"""
        pass

    @abstractmethod
    def destroy(self):
        pass

    @abstractmethod
    def power_on(self, already_on_ok=False):
        pass

    @abstractmethod
    def power_off(self, already_off_ok=False):
        pass

    @abstractmethod
    def plug_internal(self, network_name):
        pass

    @abstractmethod
    def plug_bridged(self, host_nic):
        pass

    @abstractmethod
    def unplug_all(self):
        pass

    @abstractmethod
    def setup_mac_addresses(self, make_mac):
        pass

    @abstractmethod
    def setup_network_access(self, host_ports, vm_ports):
        """Make `vm_ports` accessible from runner machine.

        Hypervisor is free to choose any strategy,
        whether it's port forwarding or virtual adapters on host machine.
        Use `.port_map` to know how to access specific port on VM.

        :param host_ports: Host ports which are allowed to use.
        :param vm_ports: Protocol, port and hint for them. Hint can be used to construct logical port numbers.
        """
        pass

    @abstractmethod
    def is_on(self):
        pass

    @abstractmethod
    def is_off(self):
        pass


class VMNotFound(Exception):
    pass


class VMAlreadyExists(Exception):
    pass


class VMAllAdaptersBusy(Exception):
    def __init__(self, vm_name, vm_networks):
        super(VMAllAdaptersBusy, self).__init__(
            "No available network adapter on {}:\n{}".format(vm_name, pformat(vm_networks)))
        self.vm_name = vm_name
        self.vm_networks = vm_networks


class VmNotReady(Exception):
    pass
