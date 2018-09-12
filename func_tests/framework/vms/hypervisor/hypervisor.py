from abc import ABCMeta, abstractmethod

from netaddr import IPAddress

from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import FileSystemPath
from framework.vms.hypervisor import VmHardware


class Hypervisor(object):
    __metaclass__ = ABCMeta

    def __init__(self, host_os_access, runner_address):  # type: (OSAccess, IPAddress) -> None
        super(Hypervisor, self).__init__()

        ## Address of machine, on which this process are executed, must be same from all
        # Mediaservers (and VMs) where tests are performed, as same update info is shared among
        # Mediaservers in system.
        #
        # It also highly desirable be known before any of VMs are created. Otherwise, update info
        # will depend on existence one of working VMs, which is cumbersome.
        self.runner_address = runner_address  # type: IPAddress

        self.host_os_access = host_os_access  # type: OSAccess

    @abstractmethod
    def import_vm(self, vm_image_path, vm_name):  # type: (FileSystemPath, str) -> VmHardware
        pass

    @abstractmethod
    def create_dummy_vm(self, vm_name, force_if_exists=False):  # type: (str) -> VmHardware
        pass

    @abstractmethod
    def find_vm(self, vm_name):  # type: (str) -> VmHardware
        pass

    @abstractmethod
    def list_vm_names(self):
        pass

    @abstractmethod
    def make_internal_network(self, network_name):
        pass
