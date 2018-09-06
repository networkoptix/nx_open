import logging
from contextlib import contextmanager
from functools import partial

from framework.os_access.exceptions import AlreadyExists
from framework.os_access.posix_access import PosixAccess
from framework.os_access.windows_access import WindowsAccess
from framework.registry import Registry
from framework.context_logger import context_logger
from framework.vms.hypervisor import VMNotFound, VmNotReady
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
        self.hardware = hardware
        self.os_access = os_access


class UnknownOsFamily(Exception):
    pass


class VMType(object):
    def __init__(
            self,
            name,
            hypervisor,
            os_family,
            power_on_timeout_sec,
            registry_path, make_name, limit,
            template_vm,
            make_mac, network_conf,
            template_url,
            template_dir,
            ):
        self._name = name
        self.hypervisor = hypervisor  # type: Hypervisor
        self.registry = Registry(
            hypervisor.host_os_access,
            registry_path,
            lambda index: make_name(vm_index=index),  # Registry doesn't know about VMs.
            limit,
            )
        self.template_vm_name = template_vm
        self.template_url = template_url
        self._template_dir = template_dir
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

    def _obtain_template(self):
        # VirtualBox sometimes locks VM for a short period of time
        # when other operation is performed in parallel.
        wait = Wait(
            "template {} not locked".format(self.template_vm_name),
            timeout_sec=30,
            logger=_logger.getChild('wait'),
            )
        while True:
            try:
                return self.hypervisor.find_vm(self.template_vm_name)
            except VMNotFound:
                if self.template_url is None:
                    raise EnvironmentError(
                        "Template VM {} not found, template VM image URL is not specified".format(
                            self.template_vm_name))
                try:
                    template_vm_image = self.hypervisor.host_os_access.download(
                        self.template_url, self._template_dir)
                except AlreadyExists as e:
                    template_vm_image = e.path
                return self.hypervisor.import_vm(template_vm_image, self.template_vm_name)
            except VmNotReady:
                if not wait.again():
                    raise
                wait.sleep()  # TODO: Need jitter on wait times.
                continue

    @contextmanager
    def vm_allocated(self, alias):
        """Allocate VM (for self-tests) bypassing any interaction with guest OS."""
        template_vm = self._obtain_template()
        with self.registry.taken(alias) as (vm_index, vm_name):
            try:
                hardware = self.hypervisor.find_vm(vm_name)
            except VMNotFound:
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
        with self.vm_allocated(alias) as vm:
            with context_logger(_logger, 'framework.networking.linux'):
                with context_logger(_logger, 'ssh'):
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
