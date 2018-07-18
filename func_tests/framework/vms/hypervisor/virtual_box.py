import logging
import re
from collections import OrderedDict
from pprint import pformat
from uuid import UUID, uuid1

from netaddr import EUI, IPNetwork
from netaddr.strategy.eui48 import mac_bare
from pylru import lrudecorator

from framework.os_access.exceptions import exit_status_error_cls
from framework.os_access.os_access_interface import OneWayPortMap, ReciprocalPortMap
from framework.vms.hypervisor import VMNotFound, VmHardware, VMAlreadyExists
from framework.vms.hypervisor.hypervisor import Hypervisor
from framework.vms.port_forwarding import calculate_forwarded_ports
from framework.waiting import wait_for_true

_logger = logging.getLogger(__name__)

_DEFAULT_QUICK_RUN_TIMEOUT_SEC = 10
_INTERNAL_NIC_INDICES = [2, 3, 4, 5, 6, 7, 8]


class VirtualBoxError(Exception):
    pass


@lrudecorator(100)
def virtual_box_error_cls(code):
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


class _VirtualBoxVm(VmHardware):
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
    def _parse_macs(raw_dict):
        for nic_index in _INTERNAL_NIC_INDICES:
            try:
                raw_mac = raw_dict['macaddress{}'.format(nic_index)]
            except KeyError:
                _logger.error("NIC %d: not present.", nic_index)
                continue
            yield nic_index, EUI(raw_mac)

    @staticmethod
    def _parse_free_nics(raw_dict, macs):
        for nic_index, mac in macs.items():
            nic_type = raw_dict['nic{}'.format(nic_index)]
            _logger.info("NIC %d (%s): %s", nic_index, mac, nic_type)
            assert nic_type != 'none', "NIC is `none` and has MAC at the same time"
            if nic_type == 'null':
                yield nic_index

    def __init__(self, virtual_box, name):
        raw_output = virtual_box.manage(['showvminfo', name, '--machinereadable'])
        raw_dict = OrderedDict(_VmInfoParser(raw_output))
        assert raw_dict['name'] == name
        _logger.info("Parse raw VM info of %s.", name)
        cls = self.__class__
        ports_map = ReciprocalPortMap(
            OneWayPortMap.forwarding(cls._parse_port_forwarding(raw_dict)),
            OneWayPortMap.direct(cls._parse_host_address(raw_dict)))
        macs = OrderedDict(cls._parse_macs(raw_dict))
        free_nics = list(cls._parse_free_nics(raw_dict, macs))
        description = raw_dict['description']
        super(_VirtualBoxVm, self).__init__(name, ports_map, macs, free_nics, description)
        self._virtual_box = virtual_box  # type: VirtualBox
        self._is_running = raw_dict['VMState'] == 'running'

    def _update(self):
        self_updated = self._virtual_box.find_vm(self.name)
        self.__dict__.update(self_updated.__dict__)

    def export(self, vm_image_path):
        """Export VM from its current state: it may not have snapshot at all"""
        self._virtual_box.manage(['export', self.name, '-o', vm_image_path, '--options', 'nomacs'])

    def clone(self, clone_vm_name):  # type: (str) -> VmHardware
        self._virtual_box.manage([
            'clonevm', self.name,
            '--snapshot', 'template',
            '--name', clone_vm_name,
            '--options', 'link',
            '--register',
            ])
        return self.__class__(self._virtual_box, clone_vm_name)

    def setup_mac_addresses(self, vm_index, mac_format):
        modify_command = ['modifyvm', self.name]
        for nic_index in [1] + _INTERNAL_NIC_INDICES:
            raw_mac = mac_format.format(vm_index=vm_index, nic_index=nic_index)
            modify_command.append('--macaddress{}={}'.format(nic_index, EUI(raw_mac, dialect=mac_bare)))
        self._virtual_box.manage(modify_command)
        self._update()

    def setup_network_access(self, vm_index, forwarded_ports_configuration):
        modify_command = ['modifyvm', self.name]
        forwarded_ports = calculate_forwarded_ports(vm_index, forwarded_ports_configuration)
        if not forwarded_ports:
            _logger.warning("No ports are forwarded to VM %s (index %d).", self.name, vm_index)
            return
        for tag, protocol, host_port, guest_port in forwarded_ports:
            modify_command.append('--natpf1={},{},,{},,{}'.format(tag, protocol, host_port, guest_port))
        self._virtual_box.manage(modify_command)
        self._update()

    def destroy(self):
        self.power_off(already_off_ok=True)
        self._virtual_box.manage(['unregistervm', self.name, '--delete'])
        _logger.info("Machine %r destroyed.", self.name)

    def is_on(self):
        self._update()
        return self._is_running

    def is_off(self):
        return not self.is_on()

    def power_on(self, already_on_ok=False):
        try:
            self._virtual_box.manage(['startvm', self.name, '--type', 'headless'])
        except virtual_box_error_cls('INVALID_OBJECT_STATE'):
            if not already_on_ok:
                raise
            return
        wait_for_true(self.is_on)

    def power_off(self, already_off_ok=False):
        try:
            self._virtual_box.manage(['controlvm', self.name, 'poweroff'])
        except VirtualBoxError as e:
            if 'is not currently running' not in e.message:
                raise
            if not already_off_ok:
                raise
            return
        wait_for_true(self.is_off)

    def plug_internal(self, network_name):
        slot = self._find_vacant_nic()
        self._virtual_box.manage([
            'controlvm', self.name,
            'nic{}'.format(slot), 'intnet', network_name])
        self._update()
        return self.macs[slot]

    def plug_bridged(self, host_nic):
        slot = self._find_vacant_nic()
        self._virtual_box.manage(['controlvm', self.name, 'nic{}'.format(slot), 'bridged', host_nic])
        self._update()
        return self.macs[slot]

    def unplug_all(self):
        self._update()
        for nic_index in self.macs.keys():
            self._virtual_box.manage(['controlvm', self.name, 'nic{}'.format(nic_index), 'null'])


class VirtualBox(Hypervisor):
    """Interface for VirtualBox as hypervisor."""

    def __init__(self, host_os_access):
        super(VirtualBox, self).__init__(host_os_access)

    def import_vm(self, vm_image_path, vm_name):
        # `import` is reserved name...
        self.manage(['import', vm_image_path, '--vsys', 0, '--vmname', vm_name], timeout_sec=600)
        self.manage(['snapshot', vm_name, 'take', 'template'])
        return self.find_vm(vm_name)

    def create_dummy_vm(self, vm_name, exists_ok=False):
        try:
            self.manage(['createvm', '--name', vm_name, '--register'])
        except VMAlreadyExists:
            if not exists_ok:
                raise
        # Without specific OS type (`Other` by default), `VBoxManage import` says:
        # "invalid ovf:id in operating system section".
        # Try: `strings ~/.func_tests/template_vm.ova` and
        # search for `OperatingSystemSection` element.
        self.manage(['modifyvm', vm_name, '--ostype', 'Ubuntu_64'])
        self.manage(['modifyvm', vm_name, '--description', 'For testing purposes. Can be deleted.'])
        return self.find_vm(vm_name)

    def find_vm(self, vm_name):  # type: (str) -> VmHardware
        return _VirtualBoxVm(self, vm_name)

    def list_vm_names(self):
        output = self.manage(['list', 'vms'])
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

    def manage(self, args, timeout_sec=_DEFAULT_QUICK_RUN_TIMEOUT_SEC):
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
            if code == 'FILE_ERROR' and 'already exists' in message:
                raise VMAlreadyExists(message)
            raise virtual_box_error_cls(code)(message)

    def make_internal_network(self, network_name):
        return '{} {} flat'.format(uuid1(), network_name)
