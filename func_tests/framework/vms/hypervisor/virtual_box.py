import csv
import logging
import re
from pprint import pformat
from uuid import UUID

from netaddr import EUI, IPNetwork
from netaddr.strategy.eui48 import mac_bare
from pylru import lrudecorator

from framework.os_access.exceptions import exit_status_error_cls
from framework.os_access.os_access_interface import OSAccess, OneWayPortMap, ReciprocalPortMap
from framework.vms.hypervisor import TemplateVMNotFound, VMAllAdaptersBusy, VMInfo, VMNotFound
from framework.vms.port_forwarding import calculate_forwarded_ports
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)

_INTERNAL_NIC_INDICES = [2, 3, 4, 5, 6, 7, 8]


class VirtualBoxError(Exception):

    def __init__(self, code, message):
        Exception.__init__(self, message)
        self.code = code
        self.message = message


@lrudecorator(100)
def virtual_box_error(code):
    assert code

    class SpecificVirtualBoxError(VirtualBoxError):
        def __init__(self, message):
            VirtualBoxError.__init__(self, code, message)

    return type('VirtualBoxError_{}'.format(code), (SpecificVirtualBoxError,), {})


def vm_info_from_raw_info(raw_info):
    ports_dict = {}
    for index in range(10):  # Arbitrary.
        try:
            raw_value = raw_info['Forwarding({})'.format(index)]
        except KeyError:
            break
        tag, protocol, host_address, host_port, guest_address, guest_port = raw_value.split(',')
        # Hostname is given with port because knowledge that VMs are accessible through
        # forwarded port is not part of logical interface. Other possibility is to make virtual network
        # in which host can access VMs on IP level. One more option is special Machine which is accessible by IP
        # and forwards ports to target VMs.
        ports_dict[protocol, int(guest_port)] = int(host_port)
    try:
        nat_network = IPNetwork(raw_info['natnet1'])
    except KeyError:
        # See: https://www.virtualbox.org/manual/ch09.html#idm8375
        nat_nic_index = 1
        nat_network = IPNetwork('10.0.{}.0/24'.format(nat_nic_index + 2))
    host_address_from_vm = nat_network.ip + 2
    ports_map = ReciprocalPortMap(
        OneWayPortMap.forwarding(ports_dict),
        OneWayPortMap.direct(host_address_from_vm))
    macs = {}
    networks = {}
    for nic_index in _INTERNAL_NIC_INDICES:
        try:
            raw_mac = raw_info['macaddress{}'.format(nic_index)]
        except KeyError:
            _logger.warning("Skip NIC %d is not present in Machine %s.", nic_index, raw_info['name'])
            continue
        macs[nic_index] = EUI(raw_mac)
        if raw_info['nic{}'.format(nic_index)] == 'null':
            networks[nic_index] = None
            _logger.debug("NIC %d (%s): empty", nic_index, macs[nic_index])
        else:
            networks[nic_index] = raw_info['intnet{}'.format(nic_index)]
            _logger.debug("NIC %d (%s): %s", nic_index, macs[nic_index], networks[nic_index])
    parsed_info = VMInfo(raw_info['name'], ports_map, macs, networks, raw_info['VMState'] == 'running')
    _logger.info("Parsed info:\n%s", pformat(parsed_info))
    return parsed_info


class VirtualBox(object):
    """Interface for VirtualBox as hypervisor."""

    def __init__(self, os_access):
        self.host_os_access = os_access  # type: OSAccess

    def _get_info(self, vm_name):
        try:
            output = self._run_vbox_manage_command(['showvminfo', vm_name, '--machinereadable'])
        except virtual_box_error('OBJECT_NOT_FOUND') as e:
            raise VMNotFound("Cannot find Machine {}; VBoxManage says:\n{}".format(vm_name, e.message))
        raw_info = dict(csv.reader(output.splitlines(), delimiter='=', escapechar='\\', doublequote=False))
        return raw_info

    def find(self, vm_name):
        raw_info = self._get_info(vm_name)
        info = vm_info_from_raw_info(raw_info)
        return info

    def clone(self, vm_name, vm_index, vm_configuration):
        """Clone and setup Machine with simple and generic parameters. Template can be any specific type."""
        try:
            self._run_vbox_manage_command([
                'clonevm', vm_configuration['template_vm'],
                '--snapshot', vm_configuration['template_vm_snapshot'],
                '--name', vm_name,
                '--options', 'link',
                '--register',
                ])
        except virtual_box_error('OBJECT_NOT_FOUND') as x:
            _logger.error('Failed to clone %r: %s', vm_name, x.message)
            mo = re.search(r"Could not find a registered machine named '(.+)'", x.message)
            if not mo:
                raise
            raise TemplateVMNotFound('Template VM is missing: %r' % mo.group(1))
        modify_command = ['modifyvm', vm_name]
        forwarded_ports = calculate_forwarded_ports(vm_index, vm_configuration['port_forwarding'])
        for tag, protocol, host_port, guest_port in forwarded_ports:
            modify_command.append('--natpf1={},{},,{},,{}'.format(tag, protocol, host_port, guest_port))
        for nic_index in [1] + _INTERNAL_NIC_INDICES:
            raw_mac = vm_configuration['mac_address_format'].format(vm_index=vm_index, nic_index=nic_index)
            modify_command.append('--macaddress{}={}'.format(nic_index, EUI(raw_mac, dialect=mac_bare)))
        self._run_vbox_manage_command(modify_command)
        raw_info = self._get_info(vm_name)
        info = vm_info_from_raw_info(raw_info)
        return info

    def destroy(self, vm_name):
        if self.find(vm_name).is_running:
            self._run_vbox_manage_command(['controlvm', vm_name, 'poweroff'])
            wait_for_true(lambda: not self.find(vm_name).is_running, 'Machine {} is off'.format(vm_name))
        self._run_vbox_manage_command(['unregistervm', vm_name, '--delete'])
        _logger.info("Machine %r destroyed.", vm_name)

    def power_on(self, vm_name):
        self._run_vbox_manage_command(['startvm', vm_name, '--type', 'headless'])

    def power_off(self, vm_name):
        self._run_vbox_manage_command(['controlvm', vm_name, 'poweroff'])

    def plug(self, vm_name, network_name):
        info = self.find(vm_name)
        try:
            slot = next(slot for slot in info.networks if info.networks[slot] is None)
        except StopIteration:
            raise VMAllAdaptersBusy(vm_name, info.networks)
        self._run_vbox_manage_command([
            'controlvm', vm_name,
            'nic{}'.format(slot), 'intnet', network_name])
        return info.macs[slot]

    def unplug_all(self, vm_name):
        info = self.find(vm_name)
        for nic_index in info.networks:
            if info.networks[nic_index] is not None:
                self._run_vbox_manage_command(
                    ['controlvm', vm_name, 'nic{}'.format(nic_index), 'null'])

    def list_vm_names(self):
        output = self._run_vbox_manage_command(['list', 'vms'])
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

    def _run_vbox_manage_command(self, args):
        try:
            return self.host_os_access.run_command(['VBoxManage'] + args)
        except exit_status_error_cls(1) as x:
            mo = re.search(r'Details: code VBOX_E_(\w+)', x.stderr)
            if not mo:
                raise
            code = mo.group(1)
            first_line = x.stderr.splitlines()[0]
            prefix = 'VBoxManage: error: '
            assert first_line.startswith(prefix)
            message = first_line[len(prefix):]
            raise virtual_box_error(code)(message)
