import logging
from collections import namedtuple
from contextlib import contextmanager

from framework.networking.linux import LinuxNetworking
from framework.networking.windows import WindowsNetworking
from framework.os_access.ssh_access import SSHAccess
from framework.os_access.winrm_access import WinRMAccess
from framework.vms.hypervisor import VMNotFound, obtain_running_vm
from framework.waiting import wait_for_true

logger = logging.getLogger(__name__)

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
                ssh_access = SSHAccess(hostname, port)
                networking = LinuxNetworking(ssh_access, info.macs.values())
                os_access = ssh_access  # Lose type information.
            elif vm_type_configuration['os_family'] == 'windows':
                winrm_access = WinRMAccess(info.ports)
                networking = WindowsNetworking(winrm_access, info.macs)
                os_access = winrm_access  # Lose type information.
            else:
                raise UnknownOsFamily("Expected 'linux' or 'windows', got %r", vm_type_configuration['os_family'])
            wait_for_true(
                os_access.is_working,
                'VM {} ({}) can be accessed via {}'.format(alias, info.name, os_access),
                timeout_sec=vm_type_configuration['power_on_timeout_sec'])
            vm = VM(alias, vm_index, vm_type, info.name, info.ports, networking, os_access)
            self._hypervisor.unplug_all(vm.name)
            vm.networking.reset()
            yield vm

    def cleanup(self):
        def destroy(name, vm_alias):
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
