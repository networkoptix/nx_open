from abc import ABCMeta, abstractmethod

from framework.os_access.os_access_interface import OSAccess


class Hypervisor(object):
    __metaclass__ = ABCMeta

    def __init__(self, host_os_access):
        super(Hypervisor, self).__init__()
        self.host_os_access = host_os_access  # type: OSAccess

    @abstractmethod
    def create_dummy(self, vm_name):
        pass

    @abstractmethod
    def find(self, vm_name):
        pass

    @abstractmethod
    def clone(self, original_vm_name, clone_vm_name):
        pass

    @abstractmethod
    def destroy(self, vm_name):
        pass

    @abstractmethod
    def power_on(self, vm_name):
        pass

    @abstractmethod
    def power_off(self, vm_name):
        pass

    @abstractmethod
    def plug(self, vm_name, network_name):
        pass

    @abstractmethod
    def unplug_all(self, vm_name):
        pass

    @abstractmethod
    def list_vm_names(self):
        pass

    @abstractmethod
    def setup_mac_addresses(self, vm_name, vm_index, mac_prefix):
        pass

    @abstractmethod
    def setup_network_access(self, vm_name, vm_index, network_access_configuration):
        pass
