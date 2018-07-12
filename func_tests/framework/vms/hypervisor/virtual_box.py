import logging
import re
from collections import OrderedDict
from pprint import pformat
from uuid import UUID

from netaddr import EUI, IPNetwork
from netaddr.strategy.eui48 import mac_bare
from pylru import lrudecorator

from framework.os_access.exceptions import exit_status_error_cls
from framework.os_access.os_access_interface import OneWayPortMap, ReciprocalPortMap
from framework.vms.hypervisor import TemplateVMNotFound, VMNotFound, VmHardware
from framework.vms.hypervisor.hypervisor import Hypervisor
from framework.vms.port_forwarding import calculate_forwarded_ports
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)

_DEFAULT_QUICK_RUN_TIMEOUT_SEC = 10
_INTERNAL_NIC_INDICES = [2, 3, 4, 5, 6, 7, 8]


class VirtualBoxError(Exception):
    pass


@lrudecorator(100)
def virtual_box_error(code):
    assert code

    class SpecificVirtualBoxError(VirtualBoxError):
        def __init__(self, message):
            super(SpecificVirtualBoxError, self).__init__(message)
            self.code = code

    return type('VirtualBoxError_{}'.format(code), (SpecificVirtualBoxError,), {})


class _VmInfoParser(object):
    """Single reason why it exists is multiline description"""

    def __init__(self, raw_output):
        self._data = raw_output.decode().replace('\r\n', '\n')
        self._pos = 0

    def _read_element(self, symbol_after):
        if self._data[self._pos] == '"':
            return self._read_quoted(symbol_after)
        else:
            return self._read_bare(symbol_after)

    def _read_bare(self, symbol_after):
        begin = self._pos
        self._pos = self._data.find(symbol_after, begin)
        end = self._pos
        self._pos += len(symbol_after)
        return self._data[begin:end]

    def _read_quoted(self, symbol_after):
        self._pos += 1
        begin = self._pos
        self._pos = self._data.find('"' + symbol_after, begin)
        end = self._pos
        self._pos += 1 + len(symbol_after)
        return self._data[begin:end]

    def _read_key(self):
        return self._read_element('=')

    def _read_value(self):
        return self._read_element('\n')

    def __iter__(self):
        while self._pos < len(self._data):
            key = self._read_key()
            value = self._read_value()
            yield key, value


class VirtualBoxVm(VmHardware):
    @staticmethod
    def _parse_port_forwarding(raw_dict):
        ports_dict = {}
        for index in range(10):  # Arbitrary.
            try:
                raw_value = raw_dict['Forwarding({})'.format(index)]
            except KeyError:
                break
            tag, protocol, host_address, host_port, guest_address, guest_port = raw_value.split(',')
            # Hostname is given with port because knowledge that VMs are accessible through
            # forwarded port is not part of logical interface. Other possibility is to make virtual network
            # in which host can access VMs on IP level. One more option is special Machine which is accessible by IP
            # and forwards ports to target VMs.
            ports_dict[protocol, int(guest_port)] = int(host_port)
        _logger.info("Forwarded ports:\n%s", pformat(ports_dict))
        return ports_dict

    @staticmethod
    def _parse_host_address(raw_dict):
        try:
            nat_network = IPNetwork(raw_dict['natnet1'])
        except KeyError:
            # See: https://www.virtualbox.org/manual/ch09.html#idm8375
            nat_nic_index = 1
            nat_network = IPNetwork('10.0.{}.0/24'.format(nat_nic_index + 2))
        host_address_from_vm = nat_network.ip + 2
        _logger.debug("Host IP network: %s.", nat_network)
        _logger.info("Host IP address: %s.", host_address_from_vm)
        return host_address_from_vm

    @staticmethod
    def _parse_nic_occupation(raw_dict):
        macs = {}
        networks = {}
        bridged = {}
        for nic_index in _INTERNAL_NIC_INDICES:
            try:
                raw_mac = raw_dict['macaddress{}'.format(nic_index)]
            except KeyError:
                _logger.error("NIC %d: not present.", nic_index)
                continue
            macs[nic_index] = EUI(raw_mac)
            nic_type = raw_dict['nic{}'.format(nic_index)]
            if nic_type == 'intnet':
                networks[nic_index] = raw_dict['intnet{}'.format(nic_index)]
                _logger.info("NIC %d (%s): internal network %s", nic_index, macs[nic_index], networks[nic_index])
            elif nic_type == 'bridged':
                bridged[nic_index] = raw_dict['bridgeadapter{}'.format(nic_index)]
                _logger.info("NIC %d (%s): bridged to %s", nic_index, macs[nic_index], bridged[nic_index])
            elif nic_type == 'null':
                networks[nic_index] = None
                _logger.info("NIC %d (%s): empty", nic_index, macs[nic_index])
            else:
                raise EnvironmentError("Unsupported NIC type: {}.".format(nic_type))
        return macs, networks, bridged

    @classmethod
    def from_raw_output(cls, raw_output):
        raw_dict = OrderedDict(_VmInfoParser(raw_output))
        name = raw_dict['name']
        _logger.info("Parse raw VM info of %s.", name)
        ports_map = ReciprocalPortMap(
            OneWayPortMap.forwarding(cls._parse_port_forwarding(raw_dict)),
            OneWayPortMap.direct(cls._parse_host_address(raw_dict)))
        macs, networks, bridged = cls._parse_nic_occupation(raw_dict)
        return cls(name, ports_map, macs, networks, bridged, raw_dict['VMState'] == 'running')


class VirtualBox(Hypervisor):
    """Interface for VirtualBox as hypervisor."""

    def __init__(self, host_os_access):
        super(VirtualBox, self).__init__(host_os_access)

    def import_vm(self, vm_image_path, vm_name):
        # `import` is reserved name...
        self._vbox_manage(['import', vm_image_path, '--vsys', 0, '--vmname', vm_name], timeout_sec=600)
        self._vbox_manage(['snapshot', vm_name, 'take', 'template'])

    def export_vm(self, vm_name, vm_image_path):
        """Export VM from its current state: it may not have snapshot at all"""
        self._vbox_manage(['export', vm_name, '-o', vm_image_path, '--options', 'nomacs'])

    def create_dummy(self, vm_name):
        self._vbox_manage(['createvm', '--name', vm_name, '--register'])
        # Without specific OS type (`Other` by default), `VBoxManage import` says:
        # "invalid ovf:id in operating system section".
        # Try: `strings ~/.func_tests/template_vm.ova` and
        # search for `OperatingSystemSection` element.
        self._vbox_manage(['modifyvm', vm_name, '--ostype', 'Ubuntu_64'])
        self._vbox_manage(['modifyvm', vm_name, '--description', 'For testing purposes. Can be deleted.'])

    def find(self, vm_name):
        output = self._vbox_manage(['showvminfo', vm_name, '--machinereadable'])
        info = VirtualBoxVm.from_raw_output(output)
        return info

    def clone(self, original_vm_name, clone_vm_name):
        try:
            self._vbox_manage([
                'clonevm', original_vm_name,
                '--snapshot', 'template',
                '--name', clone_vm_name,
                '--options', 'link',
                '--register',
                ])
        except VMNotFound as x:
            _logger.error('Failed to clone %r: %s', original_vm_name, x.message)
            mo = re.search(r"Could not find a registered machine named '(.+)'", x.message)
            if not mo:
                raise
            raise TemplateVMNotFound('Template VM is missing: %r' % mo.group(1))

    def setup_mac_addresses(self, vm_name, vm_index, mac_format):
        modify_command = ['modifyvm', vm_name]
        for nic_index in [1] + _INTERNAL_NIC_INDICES:
            raw_mac = mac_format.format(vm_index=vm_index, nic_index=nic_index)
            modify_command.append('--macaddress{}={}'.format(nic_index, EUI(raw_mac, dialect=mac_bare)))
        self._vbox_manage(modify_command)

    def setup_network_access(self, vm_name, vm_index, forwarded_ports_configuration):
        modify_command = ['modifyvm', vm_name]
        forwarded_ports = calculate_forwarded_ports(vm_index, forwarded_ports_configuration)
        if not forwarded_ports:
            _logger.warning("No ports are forwarded to VM %s (index %d).", vm_name, vm_index)
            return
        for tag, protocol, host_port, guest_port in forwarded_ports:
            modify_command.append('--natpf1={},{},,{},,{}'.format(tag, protocol, host_port, guest_port))
        self._vbox_manage(modify_command)

    def destroy(self, vm_name):
        if self.find(vm_name).is_running:
            self._vbox_manage(['controlvm', vm_name, 'poweroff'])
            wait_for_true(lambda: not self.find(vm_name).is_running, 'Machine {} is off'.format(vm_name))
        self._vbox_manage(['unregistervm', vm_name, '--delete'])
        _logger.info("Machine %r destroyed.", vm_name)

    def power_on(self, vm_name):
        self._vbox_manage(['startvm', vm_name, '--type', 'headless'])

    def power_off(self, vm_name):
        self._vbox_manage(['controlvm', vm_name, 'poweroff'])

    def plug_internal(self, vm_name, network_name):
        info = self.find(vm_name)
        slot = info.find_vacant_nic()
        self._vbox_manage([
            'controlvm', vm_name,
            'nic{}'.format(slot), 'intnet', network_name])
        return info.macs[slot]

    def plug_bridged(self, vm_name, host_nic):
        vm = self.find(vm_name)
        slot = vm.find_vacant_nic()
        self._vbox_manage(['controlvm', vm_name, 'nic{}'.format(slot), 'bridged', host_nic])
        return vm.macs[slot]

    def unplug_all(self, vm_name):
        info = self.find(vm_name)
        for nic_index in info.networks:
            if info.networks[nic_index] is not None:
                self._vbox_manage(
                    ['controlvm', vm_name, 'nic{}'.format(nic_index), 'null'])
        for nic_index in info.bridged:
            self._vbox_manage(
                ['controlvm', vm_name, 'nic{}'.format(nic_index), 'null'])

    def list_vm_names(self):
        output = self._vbox_manage(['list', 'vms'])
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

    def _vbox_manage(self, args, timeout_sec=_DEFAULT_QUICK_RUN_TIMEOUT_SEC):
        try:
            return self.host_os_access.run_command(['VBoxManage'] + args, timeout_sec=timeout_sec)
        except exit_status_error_cls(1) as x:
            first_line = x.stderr.splitlines()[0]
            prefix = 'VBoxManage: error: '
            if not first_line.startswith(prefix):
                raise VirtualBoxError(x.stderr)
            message = first_line[len(prefix):]
            mo = re.search(r'Details: code VBOX_E_(\w+)', x.stderr)
            if not mo:
                raise VirtualBoxError(message)
            code = mo.group(1)
            if code == 'OBJECT_NOT_FOUND':
                raise VMNotFound("Cannot find VM:\n{}".format(message))
            raise virtual_box_error(code)(message)
