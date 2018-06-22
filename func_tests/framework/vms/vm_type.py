from framework.vms.hypervisor import VMNotFound
from framework.vms.hypervisor.hypervisor import Hypervisor


class VMType(object):
    def __init__(
            self,
            hypervisor,
            template_vm, template_vm_snapshot,
            mac_address_format, port_forwarding):
        self.hypervisor = hypervisor  # type: Hypervisor
        self.template_vm_name = template_vm
        self._snapshot_name = template_vm_snapshot
        self.mac_format = mac_address_format
        self.network_access_configuration = port_forwarding

    def obtain(self, vm_name, vm_index):
        try:
            vm_info = self.hypervisor.find(vm_name)
        except VMNotFound:
            self.hypervisor.clone(self.template_vm_name, self._snapshot_name, vm_name)
            self.hypervisor.setup_mac_addresses(vm_name, vm_index, self.mac_format)
            self.hypervisor.setup_network_access(vm_name, vm_index, self.network_access_configuration)
            vm_info = self.hypervisor.find(vm_name)
        assert vm_info.name == vm_name
        if not vm_info.is_running:
            self.hypervisor.power_on(vm_info.name)
        return vm_info
