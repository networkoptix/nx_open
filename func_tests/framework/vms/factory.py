import logging
from contextlib import contextmanager

from framework.os_access.ssh_access import VmSshAccess
from framework.os_access.windows_access import WindowsAccess
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)


class VM(object):
    def __init__(self, alias, index, type, hardware, os_access):
        self.alias = alias
        self.index = index
        self.type = type
        self.name = hardware.name  # TODO: Remove.
        self.port_map = hardware.port_map  # TODO: Remove.
        self.hardware = hardware
        self.os_access = os_access


class UnknownOsFamily(Exception):
    pass


class VMFactory(object):
    def __init__(self, vm_configuration, hypervisor, vm_types):
        self._vm_configuration = vm_configuration
        self._hypervisor = hypervisor
        self._vm_types = vm_types

    @contextmanager
    def allocated_vm(self, alias, vm_type='linux'):
        with self._vm_types[vm_type].obtained(alias) as (info, vm_index):
            vm_type_configuration = self._vm_configuration[vm_type]
            username, password, key = info.description.split('\n', 2)
            if vm_type_configuration['os_family'] == 'linux':
                os_access = VmSshAccess(alias, info.port_map, info.macs, username, key)
            elif vm_type_configuration['os_family'] == 'windows':
                os_access = WindowsAccess(alias, info.port_map, info.macs, username, password)
            else:
                raise UnknownOsFamily("Expected 'linux' or 'windows', got %r", vm_type_configuration['os_family'])
            wait_for_true(os_access.is_accessible, timeout_sec=vm_type_configuration['power_on_timeout_sec'])
            # TODO: Consider unplug and reset only before network setup: that will make tests much faster.
            info.unplug_all()
            os_access.networking.reset()
            vm = VM(alias, vm_index, vm_type, info, os_access)
            yield vm

    def cleanup(self):
        for vm_type in self._vm_types.values():
            vm_type.cleanup()
