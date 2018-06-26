import logging
from contextlib import contextmanager

from framework.os_access.exceptions import AlreadyDownloaded
from framework.registry import Registry
from framework.vms.hypervisor import TemplateVMNotFound, VMNotFound
from framework.vms.hypervisor.hypervisor import Hypervisor

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
        try:
            self.hypervisor.find(self.template_vm_name)
        except VMNotFound:
            if self.template_url is None:
                raise TemplateVMNotFound(
                    "Template VM {} not found, template VM image URL is not specified".format(
                        self.template_vm_name))
            template_vm_images_dir = self.hypervisor.host_os_access.Path.home() / '.func_tests'
            try:
                template_vm_image = self.hypervisor.host_os_access.download(self.template_url, template_vm_images_dir)
            except AlreadyDownloaded as e:
                template_vm_image = e.path
            self.hypervisor.import_vm(template_vm_image, self.template_vm_name)

    @contextmanager
    def obtained(self, alias):
        self._obtain_template()
        with self.registry.taken(alias) as (vm_index, vm_name):
            try:
                vm_info = self.hypervisor.find(vm_name)
            except VMNotFound:
                self.hypervisor.clone(self.template_vm_name, vm_name)
                self.hypervisor.setup_mac_addresses(vm_name, vm_index, self.mac_format)
                self.hypervisor.setup_network_access(vm_name, vm_index, self.network_access_configuration)
                vm_info = self.hypervisor.find(vm_name)
            assert vm_info.name == vm_name
            if not vm_info.is_running:
                self.hypervisor.power_on(vm_info.name)
            yield vm_info, vm_index

    def cleanup(self):
        def destroy(name, vm_alias):
            if vm_alias is None:
                try:
                    self.hypervisor.destroy(name)
                except VMNotFound:
                    _logger.info("Machine %s not found.", name)
                else:
                    _logger.info("Machine %s destroyed.", name)
            else:
                _logger.warning("Machine %s reserved now.", name)

        self.registry.for_each(destroy)
