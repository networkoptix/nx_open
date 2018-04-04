import logging
from collections import namedtuple

from framework.networking.linux import LinuxNetworking
from framework.os_access.ssh import SSHAccess
from framework.os_access.windows_remoting.winrm_access import WinRMAccess
from framework.utils import wait_until
from framework.vms.hypervisor import VMNotFound

logger = logging.getLogger(__name__)


class VMNotResponding(Exception):
    def __init__(self, alias, name):
        super(VMNotResponding, self).__init__("Machine {} ({}) is not responding".format(name, alias))
        self.alias = alias
        self.name = name


VM = namedtuple('VM', ['alias', 'index', 'name', 'ports', 'networking', 'os_access'])


class AccessTypeUnknown(Exception):
    pass


def make_os_access(access_type, vm_ports):
    if access_type == 'ssh':
        hostname, port = vm_ports['tcp', 22]
        return SSHAccess(hostname, port)
    if access_type == 'winrm':
        hostname, port = vm_ports['tcp', 5985]
        return WinRMAccess(hostname, port)
    raise AccessTypeUnknown("Access must be 'ssh' or 'winrm' but is {}".format(access_type))


class VMFactory(object):
    def __init__(self, vm_configuration, hypervisor, registry):
        self._vm_configuration = vm_configuration
        self._hypervisor = hypervisor
        self._registry = registry

    def find_or_clone(self, vm_index):
        name = self._vm_configuration['name_format'].format(vm_index=vm_index)
        try:
            info = self._hypervisor.find(name)
        except VMNotFound:
            info = self._hypervisor.clone(name, vm_index, self._vm_configuration['vm'])
        assert info.name == name
        if not info.is_running:
            self._hypervisor.power_on(info.name)
        return info

    def allocate(self, alias):
        index = self._registry.reserve(alias)
        info = self.find_or_clone(index)
        os_access = make_os_access(self._vm_configuration['access_type'], info.ports)
        if not wait_until(
                os_access.is_working,
                name='until {} ({}) can be accesses via {!r}'.format(alias, info.name, os_access),
                timeout_sec=self._vm_configuration['power_on_timeout_sec']):
            raise VMNotResponding(alias, info.name)
        networking = LinuxNetworking(os_access, info.macs.values())
        vm = VM(alias, index, info.name, info.ports, networking, os_access)
        self._hypervisor.unplug_all(vm.name)
        vm.networking.reset()
        vm.networking.enable_internet()
        return vm

    def release(self, vm):
        self._registry.relinquish(vm.index)

    def cleanup(self):
        def destroy(vm_index, vm_alias):
            name = self._vm_configuration['name_format'].format(vm_index=vm_index)
            if vm_alias is None:
                try:
                    self._hypervisor.destroy(name)
                except VMNotFound:
                    logger.info("VM %s not found.", name)
                else:
                    logger.info("VM %s destroyed.", name)
            else:
                logger.warning("VM %s reserved now.", name)

        self._registry.for_each(destroy)
