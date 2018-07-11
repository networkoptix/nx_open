"""Hypervisor somehow provides network access to VMs. It can be done via port forwarding."""
from collections import namedtuple


class PortForwardingConfigurationError(Exception):
    pass


ForwardedPort = namedtuple('ForwardedPort', ['tag', 'protocol', 'host_port', 'guest_port'])


def calculate_forwarded_ports(vm_index, vm_configuration):
    forwarded_ports = []
    for protocol_target, host_port_offset in vm_configuration['vm_ports_to_host_port_offsets'].items():
        host_port_base = vm_configuration['host_ports_base'] + vm_index * vm_configuration['host_ports_per_vm']
        protocol, guest_port_str = protocol_target.split('/')
        guest_port = int(guest_port_str)
        host_port = host_port_base + host_port_offset
        if not 0 <= host_port_offset < vm_configuration['host_ports_per_vm']:
            raise PortForwardingConfigurationError(
                "Host port offset must be in [0, {:d}) but is {:d}".format(
                    vm_configuration['host_ports_per_vm'], host_port_offset))
        forwarded_ports.append(
            ForwardedPort(
                tag=protocol_target,
                protocol=protocol,
                host_port=host_port,
                guest_port=guest_port))
    return forwarded_ports
