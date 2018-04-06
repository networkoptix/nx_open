import logging
from collections import namedtuple
from contextlib import contextmanager

from framework.networking.linux import LinuxNetworking
from framework.networking.windows import WindowsNetworking
from framework.os_access.ssh import SSHAccess
from framework.os_access.windows_remoting.winrm_access import WinRMAccess
from framework.utils import wait_until
from framework.vms.hypervisor import VMNotFound, obtain_running_vm

logger = logging.getLogger(__name__)


class VMNotResponding(Exception):
    def __init__(self, alias, name):
        super(VMNotResponding, self).__init__("Machine {} ({}) is not responding".format(name, alias))
        self.alias = alias
        self.name = name


VM = namedtuple('VM', ['alias', 'index', 'type', 'name', 'ports', 'networking', 'os_access'])


class UnknownOsFamily(Exception):
    pass


class VMFactory(object):
    def __init__(self, vm_configuration, hypervisor, registries):
        self._vm_configuration = vm_configuration
        self._hypervisor = hypervisor
        self._registries = registries

    @contextmanager
    def allocated_vm(self, alias, vm_type='linux'):
        with self._registries[vm_type].taken(alias) as (vm_index, name):
            vm_type_configuration = self._vm_configuration[vm_type]
            info = obtain_running_vm(self._hypervisor, name, vm_index, vm_type_configuration['vm'])
            if vm_type_configuration['os_family'] == 'linux':
                hostname, port = info.ports['tcp', 22]
                os_access = SSHAccess(hostname, port)
                networking = LinuxNetworking(os_access, info.macs.values())
            elif vm_type_configuration['os_family'] == 'windows':
                hostname, port = info.ports['tcp', 5985]
                os_access = WinRMAccess(hostname, port)
                networking = WindowsNetworking(os_access, info.macs.values())
            else:
                raise UnknownOsFamily("Expected 'linux' or 'windows', got %r", vm_type_configuration['os_family'])
            if not wait_until(
                    os_access.is_working,
                    name='until {} ({}) can be accesses via {!r}'.format(alias, info.name, os_access),
                    timeout_sec=vm_type_configuration['power_on_timeout_sec']):
                raise VMNotResponding(alias, info.name)
            vm = VM(alias, vm_index, vm_type, info.name, info.ports, networking, os_access)
            self._hypervisor.unplug_all(vm.name)
            vm.networking.reset()
            vm.networking.enable_internet()
            yield vm

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

        for registry in self._registries.values():
            registry.for_each(destroy)
