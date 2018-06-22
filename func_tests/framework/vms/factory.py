import logging
from collections import namedtuple
from contextlib import contextmanager

from framework.os_access.local_path import LocalPath
from framework.os_access.ssh_access import SSHAccess
from framework.os_access.windows_access import WindowsAccess
from framework.vms.hypervisor import VMNotFound
from framework.vms.vm_type import VMType
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)

SSH_PRIVATE_KEY_PATH = LocalPath.home() / '.func_tests' / 'ssh' / 'private_key'  # Get from VM's description.

VM = namedtuple('VM', ['alias', 'index', 'type', 'name', 'port_map', 'os_access'])


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
            if vm_type_configuration['os_family'] == 'linux':
                os_access = SSHAccess(info.port_map, info.macs, 'root', SSH_PRIVATE_KEY_PATH)
            elif vm_type_configuration['os_family'] == 'windows':
                os_access = WindowsAccess(info.port_map, info.macs, u'Administrator', u'qweasd123')
            else:
                raise UnknownOsFamily("Expected 'linux' or 'windows', got %r", vm_type_configuration['os_family'])
            wait_for_true(os_access.is_accessible, timeout_sec=vm_type_configuration['power_on_timeout_sec'])
            # TODO: Consider unplug and reset only before network setup: that will make tests much faster.
            self._hypervisor.unplug_all(info.name)
            os_access.networking.reset()
            vm = VM(alias, vm_index, vm_type, info.name, info.port_map, os_access)
            yield vm

    def cleanup(self):
        def destroy(name, vm_alias):
            if vm_alias is None:
                try:
                    self._hypervisor.destroy(name)
                except VMNotFound:
                    _logger.info("Machine %s not found.", name)
                else:
                    _logger.info("Machine %s destroyed.", name)
            else:
                _logger.warning("Machine %s reserved now.", name)

        for vm_type in self._vm_types.values():
            vm_type.registry.for_each(destroy)
