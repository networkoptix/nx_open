import logging
from contextlib import contextmanager

from framework.os_access.exceptions import AlreadyDownloaded
from framework.os_access.ssh_access import VmSshAccess
from framework.os_access.windows_access import WindowsAccess
from framework.registry import Registry
from framework.vms.hypervisor import VMNotFound, VmNotReady
from framework.vms.hypervisor.hypervisor import Hypervisor
from framework.waiting import Wait, wait_for_true

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
            hypervisor,
            os_family,
            power_on_timeout_sec,
            registry_path, name_format, limit,
            template_vm,
            mac_address_format, port_forwarding,
            template_url=None,
            ):
        self.hypervisor = hypervisor  # type: Hypervisor
        self.registry = Registry(
            hypervisor.host_os_access,
            registry_path,
            name_format.format(vm_index='{index}'),  # Registry doesn't know about VMs.
            limit,
            )
        self.template_vm_name = template_vm
        self.template_url = template_url
        self.mac_format = mac_address_format
        self.network_access_configuration = port_forwarding
        self._power_on_timeout_sec = power_on_timeout_sec
        self._os_family = os_family

    def _obtain_template(self):
        # VirtualBox sometimes locks VM for a short period of time when other operation is performed in parallel.
        wait = Wait("template {} not locked".format(self.template_vm_name), timeout_sec=30)
        while True:
            try:
                return self.hypervisor.find_vm(self.template_vm_name)
            except VMNotFound:
                if self.template_url is None:
                    raise EnvironmentError(
                        "Template VM {} not found, template VM image URL is not specified".format(
                            self.template_vm_name))
                template_vm_images_dir = self.hypervisor.host_os_access.Path.home() / '.func_tests'
                try:
                    template_vm_image = self.hypervisor.host_os_access.download(self.template_url, template_vm_images_dir)
                except AlreadyDownloaded as e:
                    template_vm_image = e.path
                return self.hypervisor.import_vm(template_vm_image, self.template_vm_name)
            except VmNotReady:
                if not wait.again():
                    raise
                wait.sleep()  # TODO: Need jitter on wait times.
                continue

    @contextmanager
    def obtained(self, alias):
        template_vm = self._obtain_template()
        with self.registry.taken(alias) as (vm_index, vm_name):
            try:
                vm = self.hypervisor.find_vm(vm_name)
            except VMNotFound:
                vm = template_vm.clone(vm_name)
                vm.setup_mac_addresses(vm_index, self.mac_format)
                vm.setup_network_access(vm_index, self.network_access_configuration)
            vm.power_on(already_on_ok=True)
            yield vm, vm_index

    @contextmanager
    def allocated_vm(self, alias):
        with self.obtained(alias) as (vm, vm_index):
            username, password, key = vm.description.split('\n', 2)
            if self._os_family == 'linux':
                os_access = VmSshAccess(alias, vm.port_map, vm.macs, username, key)
            elif self._os_family == 'windows':
                os_access = WindowsAccess(alias, vm.port_map, vm.macs, username, password)
            else:
                raise UnknownOsFamily("Expected 'linux' or 'windows', got %r", self._os_family)
            wait_for_true(os_access.is_accessible, timeout_sec=self._power_on_timeout_sec)
            # TODO: Consider unplug and reset only before network setup: that will make tests much faster.
            vm.unplug_all()
            os_access.networking.reset()
            vm = VM(alias, vm_index, self._os_family, vm, os_access)
            yield vm

    def cleanup(self):
        def destroy(name, vm_alias):
            if vm_alias is not None:
                _logger.warning("Machine %s reserved now.", name)
                return
            try:
                vm = self.hypervisor.find_vm(name)
            except VMNotFound:
                _logger.info("Machine %s not found.", name)
                return
            vm.destroy()
            _logger.info("Machine %s destroyed.", name)

        self.registry.for_each(destroy)
