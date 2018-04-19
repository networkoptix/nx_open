import csv
import logging
from pprint import pformat
from uuid import UUID

from netaddr import EUI
from netaddr.strategy.eui48 import mac_bare

from framework.os_access.exceptions import exit_status_error_cls
from framework.vms.hypervisor import VMAllAdaptersBusy, VMInfo, VMNotFound
from framework.vms.port_forwarding import calculate_forwarded_ports
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)

_INTERNAL_NIC_INDICES = [2, 3, 4, 5, 6, 7, 8]


def vm_info_from_raw_info(host_hostname, raw_info):
    ports = {}
    for index in range(10):  # Arbitrary.
        try:
            raw_value = raw_info['Forwarding({})'.format(index)]
        except KeyError:
            break
        tag, protocol, host_address, host_port, guest_address, guest_port = raw_value.split(',')
        # Hostname is given with port because knowledge that VMs are accessible through
        # forwarded port is not part of logical interface. Other possibility is to make virtual network
        # in which host can access VMs on IP level. One more option is special VM which is accessible by IP
        # and forwards ports to target VMs.
        ports[protocol, int(guest_port)] = host_hostname, int(host_port)
    macs = {}
    networks = {}
    for nic_index in _INTERNAL_NIC_INDICES:
        try:
            raw_mac = raw_info['macaddress{}'.format(nic_index)]
        except KeyError:
            _logger.warning("Skip NIC %d is not present in VM %s.", nic_index, raw_info['name'])
            continue
        macs[nic_index] = EUI(raw_mac)
        if raw_info['nic{}'.format(nic_index)] == 'null':
            networks[nic_index] = None
            _logger.debug("NIC %d (%s): empty", nic_index, macs[nic_index])
        else:
            networks[nic_index] = raw_info['intnet{}'.format(nic_index)]
            _logger.debug("NIC %d (%s): %s", nic_index, macs[nic_index], networks[nic_index])
    parsed_info = VMInfo(raw_info['name'], ports, macs, networks, raw_info['VMState'] == 'running')
    _logger.info("Parsed info:\n%s", pformat(parsed_info))
    return parsed_info


class VirtualBox(object):
    """Interface for VirtualBox as hypervisor."""

    def __init__(self, host_os_access, host_hostname):
        self.host_hostname = host_hostname
        self.host_os_access = host_os_access

    def _get_info(self, vm_name):
        try:
            output = self.host_os_access.run_command(['VBoxManage', 'showvminfo', vm_name, '--machinereadable'])
        except exit_status_error_cls(1) as e:
            raise VMNotFound("Cannot find VM {}; VBoxManage says:\n{}".format(vm_name, e.stderr))
        raw_info = dict(csv.reader(output.splitlines(), delimiter='=', escapechar='\\', doublequote=False))
        return raw_info

    def find(self, vm_name):
        raw_info = self._get_info(vm_name)
        info = vm_info_from_raw_info(self.host_hostname, raw_info)
        return info

    def clone(self, vm_name, vm_index, vm_configuration):
        """Clone and setup VM with simple and generic parameters. Template can be any specific type."""
        self.host_os_access.run_command([
            'VBoxManage', 'clonevm', vm_configuration['template_vm'],
            '--snapshot', vm_configuration['template_vm_snapshot'],
            '--name', vm_name,
            '--options', 'link',
            '--register',
            ])
        modify_command = [
            'VBoxManage', 'modifyvm', vm_name,
            '--paravirtprovider=kvm',
            '--cpus=2', '--cpuexecutioncap=100', '--memory=2048',
            '--nic1=nat', '--natnet1=192.168.254.0/24',
            ]
        forwarded_ports = calculate_forwarded_ports(vm_index, vm_configuration['port_forwarding'])
        for tag, protocol, host_port, guest_port in forwarded_ports:
            modify_command.append('--natpf1={},{},,{},,{}'.format(tag, protocol, host_port, guest_port))
        for nic_index in _INTERNAL_NIC_INDICES:
            modify_command.append('--nic{}=null'.format(nic_index))
        for nic_index in [1] + _INTERNAL_NIC_INDICES:
            raw_mac = vm_configuration['mac_address_format'].format(vm_index=vm_index, nic_index=nic_index)
            modify_command.append('--macaddress{}={}'.format(nic_index, EUI(raw_mac, dialect=mac_bare)))
        self.host_os_access.run_command(modify_command)
        extra_data = {'VBoxInternal/Devices/VMMDev/0/Config/GetHostTimeDisabled': 1}
        for extra_key, extra_value in extra_data.items():
            self.host_os_access.run_command(['VBoxManage', 'setextradata', vm_name, extra_key, extra_value])
        raw_info = self._get_info(vm_name)
        info = vm_info_from_raw_info(self.host_hostname, raw_info)
        return info

    def destroy(self, vm_name):
        if self.find(vm_name).is_running:
            self.host_os_access.run_command(['VBoxManage', 'controlvm', vm_name, 'poweroff'])
            wait_for_true(lambda: not self.find(vm_name).is_running, 'VM {} is off'.format(vm_name))
        self.host_os_access.run_command(['VBoxManage', 'unregistervm', vm_name, '--delete'])
        _logger.info("VM %r destroyed.", vm_name)

    def power_on(self, vm_name):
        self.host_os_access.run_command(['VBoxManage', 'startvm', vm_name, '--type', 'headless'])

    def power_off(self, vm_name):
        self.host_os_access.run_command(['VBoxManage', 'controlvm', vm_name, 'poweroff'])

    def plug(self, vm_name, network_name):
        info = self.find(vm_name)
        try:
            slot = next(slot for slot in info.networks if info.networks[slot] is None)
        except StopIteration:
            raise VMAllAdaptersBusy(vm_name, info.networks)
        self.host_os_access.run_command([
            'VBoxManage', 'controlvm', vm_name,
            'nic{}'.format(slot), 'intnet', network_name])
        return info.macs[slot]

    def unplug_all(self, vm_name):
        info = self.find(vm_name)
        for nic_index in info.networks:
            if info.networks[nic_index] is not None:
                self.host_os_access.run_command(['VBoxManage', 'controlvm', vm_name, 'nic{}'.format(nic_index), 'null'])

    def list_vm_names(self):
        output = self.host_os_access.run_command(['VBoxManage', 'list', 'vms'])
        lines = output.strip().splitlines()
        vm_names = []
        for line in lines:
            # No chars are escaped in name. It's just enclosed in double quotes.
            quoted_name, uuid_str = line.rsplit(' ', 1)
            uuid = UUID(hex=uuid_str)
            assert uuid != UUID(int=0)  # Only check that it's valid.
            name = quoted_name[1:-1]
            vm_names.append(name)
        return vm_names
