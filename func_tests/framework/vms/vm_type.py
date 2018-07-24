import logging
from contextlib import contextmanager

from framework.os_access.exceptions import AlreadyDownloaded
from framework.registry import Registry
from framework.vms.hypervisor import VMNotFound, VmNotReady
from framework.vms.hypervisor.hypervisor import Hypervisor
from framework.waiting import Wait

_logger = logging.getLogger(__name__)


class VMType(object):
    def __init__(
            self,
            hypervisor,
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

    def _obtain_template(self):
        # VirtualBox sometimes locks VM for a short period of time when other operation is performed in parallel.
        wait = Wait("template not locked", timeout_sec=30)
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
                wait.sleep()
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
