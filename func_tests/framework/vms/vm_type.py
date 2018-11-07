import logging
from contextlib import contextmanager
from functools import partial

from contextlib2 import ExitStack
from parse import parse

from framework.context_logger import context_logger
from framework.os_access.exceptions import AlreadyExists
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.path import FileSystemPath
from framework.os_access.posix_access import PosixAccess
from framework.os_access.windows_access import WindowsAccess
from framework.registry import Registry
from framework.vms.hypervisor import VMNotFound, VmHardware, VmNotReady
from framework.vms.hypervisor.hypervisor import Hypervisor
from framework.waiting import Wait, WaitTimeout, wait_for_truthy

_logger = logging.getLogger(__name__)


class VM(object):
    def __init__(self, alias, index, type, hardware, os_access):
        self.alias = alias
        self.index = index
        self.type = type
        self.name = hardware.name  # TODO: Remove.
        self.port_map = hardware.port_map  # TODO: Remove.
        self.hardware = hardware  # type: VmHardware
        self.os_access = os_access  # type: OSAccess


class UnknownOsFamily(Exception):
    pass


class VmTemplate(object):
    def __init__(self, hypervisor, vm_name, url, downloads_dir):
        # type: (Hypervisor, str, str, FileSystemPath) -> ...
        self._hypervisor = hypervisor
        self._vm_name = vm_name
        self._url = url
        self._downloads_dir = downloads_dir

    def arrange_template_vm(self):
        # VirtualBox sometimes locks VM for a short period of time
        # when other operation is performed in parallel.
        wait = Wait(
            "template {} not locked".format(self._vm_name),
            timeout_sec=30,
            logger=_logger.getChild('wait'),
            )
        while True:
            try:
                return self._hypervisor.find_vm(self._vm_name)
            except VMNotFound:
                if self._url is None:
                    raise EnvironmentError(
                        "Template VM {} not found, template VM image URL is not specified".format(
                            self._vm_name))
                vm_image = self._hypervisor.host_os_access.download(self._url, self._downloads_dir)
                return self._hypervisor.import_vm(vm_image, self._vm_name)
            except VmNotReady:
                if not wait.again():
                    raise
                wait.sleep()  # TODO: Need jitter on wait times.
                continue


class VMType(object):
    def __init__(
            self,
            name,
            hypervisor,
            os_family,
            power_on_timeout_sec,
            registry_path, make_name, limit,
            make_mac, network_conf,
            template,  # type: VmTemplate
            ):
        self._name = name
        self.hypervisor = hypervisor  # type: Hypervisor
        self.registry = Registry(
            hypervisor.host_os_access,
            registry_path,
            lambda index: make_name(vm_index=index),  # Registry doesn't know about VMs.
            limit,
            )
        self._template = template
        self._make_mac = make_mac
        self._port_offsets = network_conf['vm_ports_to_host_port_offsets']
        self._ports_per_vm = network_conf['host_ports_per_vm']
        self._ports_base = network_conf['host_ports_base']
        self._power_on_timeout_sec = power_on_timeout_sec
        self._os_family = os_family

    def __str__(self):
        return 'VMType {!r}'.format(self._name)

    def __repr__(self):
        return '<{!s}>'.format(self)

    @classmethod
    def from_config(cls, hypervisor, name, conf, template, slot=0):
        return cls(
            name,
            hypervisor,
            conf['os_family'],
            conf['power_on_timeout_sec'],
            conf['vm']['registry_path'].format(slot=slot),
            partial(conf['vm']['name_format'].format, slot=slot),
            conf['vm']['machines_per_slot'],
            partial(conf['vm']['mac_address_format'].format, slot=slot),
            {
                'host_ports_base': (
                        conf['vm']['port_forwarding']['host_ports_base']
                        + (
                                slot
                                * conf['vm']['machines_per_slot']
                                * conf['vm']['port_forwarding']['host_ports_per_vm']
                        )
                ),
                'host_ports_per_vm': conf['vm']['port_forwarding']['host_ports_per_vm'],
                'vm_ports_to_host_port_offsets': {
                    parse('{}/{:d}', key): hint
                    for key, hint
                    in conf['vm']['port_forwarding']['vm_ports_to_host_port_offsets'].items()
                    },
                },
            template,
            )

    @contextmanager
    def vm_allocated(self, alias):
        """Allocate VM (for self-tests) bypassing any interaction with guest OS."""
        with self.registry.taken(alias) as (vm_index, vm_name):
            try:
                hardware = self.hypervisor.find_vm(vm_name)
            except VMNotFound:
                template_vm = self._template.arrange_template_vm()
                hardware = template_vm.clone(vm_name)
                hardware.setup_mac_addresses(partial(self._make_mac, vm_index=vm_index))
                ports_base_for_vm = self._ports_base + self._ports_per_vm * vm_index
                ports_for_vm = range(ports_base_for_vm, ports_base_for_vm + self._ports_per_vm)
                hardware.setup_network_access(ports_for_vm, self._port_offsets)
            username, password, key = hardware.description.split('\n', 2)
            if self._os_family == 'linux':
                os_access = PosixAccess.to_vm(
                    alias, hardware.port_map, hardware.macs, username, key)
            elif self._os_family == 'windows':
                os_access = WindowsAccess(
                    alias, hardware.port_map, hardware.macs, username, password)
            else:
                raise UnknownOsFamily("Expected 'linux' or 'windows', got %r", self._os_family)
            yield VM(alias, vm_index, self._os_family, hardware, os_access)

    @contextmanager
    def vm_ready(self, alias):
        """Get accessible, cleaned up add ready-to-use VM."""
        with self.vm_allocated(alias) as vm:  # type: VM
            with ExitStack() as stack:
                stack.enter_context(context_logger(_logger, 'framework.networking.linux'))
                stack.enter_context(context_logger(_logger, 'framework.networking.windows'))
                stack.enter_context(context_logger(_logger, 'ssh'))
                stack.enter_context(context_logger(_logger, 'framework.os_access.windows_remoting'))

                vm.hardware.unplug_all()
                recovering_timeouts_sec = vm.hardware.recovering(self._power_on_timeout_sec)
                for timeout_sec in recovering_timeouts_sec:
                    try:
                        wait_for_truthy(
                            vm.os_access.is_accessible,
                            timeout_sec=timeout_sec,
                            logger=_logger.getChild('wait'),
                            )
                    except WaitTimeout:
                        continue
                    break
                # Networking reset is quite lengthy operation.
                # TODO: Consider unplug and reset only before network setup.
                vm.os_access.networking.reset()
                vm.os_access.cleanup_disk_space()
                vm.hardware.reset_bandwidth()
                yield vm

    def cleanup(self):
        """Cleanup all VMs, fail if locked VM encountered.

        It's intended to be used only when there are no other runs.
        """
        _logger.info('%s: Cleaning all VMs', self)
        for index, name, lock_path in self.registry.possible_entries():
            with self.hypervisor.host_os_access.lock_acquired(lock_path, timeout_sec=2):
                try:
                    vm = self.hypervisor.find_vm(name)
                except VMNotFound:
                    _logger.debug("Machine %s not found.", name)
                    continue
                vm.destroy()
