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
from framework.port_check import port_is_open
from framework.vms.hypervisor import VMAlreadyExists, VMNotFound, VmHardware, VmNotReady
from framework.vms.hypervisor.hypervisor import Hypervisor
from framework.waiting import wait_for_truthy

_logger = logging.getLogger(__name__)

_DEFAULT_QUICK_RUN_TIMEOUT_SEC = 10

## This NIC is used for
# - remote OS setup,
# - working with remote filesystem,
# - installing Mediaserver.
#
# Ports are forwarded to this NIC.
_MANAGEMENT_NIC_INDEX = 1

## This NIC is used for connections initiated from within VM, e.g. when tests runner is serving
# data by HTTP and Mediaserver is downloading it. Such traffic goes through other NIC to make it
# possible to limit it by means of VirtualBox.
_HOST_CONNECTION_NIC_INDEX = 2

## Others are used to combine VMs into internal networks. These are unplugged before every test.
_INTERNAL_NIC_INDICES = [3, 4, 5, 6, 7, 8]

## All NIC indices. Currently used only to setup MAC addresses of the newly cloned VM.
# Theoretically, some NICs may not be used at all bur their indices should be listed here to get
# non-random MAC address.
_ALL_NIC_INDICES = [_MANAGEMENT_NIC_INDEX, _HOST_CONNECTION_NIC_INDEX] + _INTERNAL_NIC_INDICES


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


class VmUnresponsive(Exception):
    pass


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


class _ForwardedPort(object):
    def __init__(self, tag, protocol, host_address, host_port, guest_address, guest_port):
        self.tag = tag
        self.protocol = protocol
        self.host_address = host_address
        self.host_port = host_port
        self.guest_address = guest_address
        self.guest_port = guest_port

    def __repr__(self):
        return '<{} {}/{} to {}>'.format(
            self.__class__.__name__, self.protocol, self.guest_port, self.host_port)

    @classmethod
    def new(cls, protocol, guest_port, host_port):
        tag = '{}/{}'.format(protocol, guest_port)
        return cls(tag, protocol, '', host_port, '', guest_port)

    @classmethod
    def from_conf(cls, conf):
        tag, protocol, host_address, host_port_str, guest_address, guest_port_str = conf.split(',')
        return cls(
            tag, protocol,
            host_address, int(host_port_str),
            guest_address, int(guest_port_str))

    def conf(self):
        return ','.join((
            self.tag, self.protocol,
            self.host_address, str(self.host_port),
            self.guest_address, str(self.guest_port)))


class _VirtualBoxVm(VmHardware):
    @staticmethod
    def _parse_port_forwarding(raw_dict):
        for index in range(10):  # Arbitrary.
            try:
                yield _ForwardedPort.from_conf(raw_dict['Forwarding({})'.format(index)])
            except KeyError:
                break

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
            _logger.debug("NIC %d (%s): %s", nic_index, mac, nic_type)
            assert nic_type != 'none', "NIC is `none` and has MAC at the same time"
            if nic_type == 'null':
                yield nic_index

    def __init__(self, virtual_box, name):  # type: (VirtualBox, str) -> None
        raw_output = virtual_box.manage(['showvminfo', name, '--machinereadable'])
        _logger.debug("Parse raw VM info of %s.", name)
        raw_dict = OrderedDict(_VmInfoParser(raw_output))
        assert raw_dict['name'] == name
        is_running = raw_dict['VMState'] == 'running'
        _logger.debug('VM %s: %s', name, 'running' if is_running else 'stopped')
        cls = self.__class__
        self._port_forwarding = list(cls._parse_port_forwarding(raw_dict))
        _logger.debug("VM %s: forwarded ports:\n%s", name, pformat(self._port_forwarding))
        ports_map = ReciprocalPortMap(
            OneWayPortMap.forwarding({
                (port.protocol, port.guest_port): port.host_port
                for port in self._port_forwarding}),
            OneWayPortMap.direct(virtual_box.runner_address))
        macs = OrderedDict(cls._parse_macs(raw_dict))
        free_nics = list(cls._parse_free_nics(raw_dict, macs))
        try:
            description = raw_dict['description']
        except KeyError:
            raise VmNotReady(
                "`VBoxManage showvminfo` omitted description; "
                "it may happen when VM is accessed too frequently")
        super(_VirtualBoxVm, self).__init__(name, ports_map, macs, free_nics, description)
        self._virtual_box = virtual_box  # type: VirtualBox
        self._is_running = is_running

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

    def setup_mac_addresses(self, make_mac):
        modify_command = ['modifyvm', self.name]
        for nic_index in _ALL_NIC_INDICES:
            raw_mac = make_mac(nic_index=nic_index)
            mac = EUI(raw_mac, dialect=mac_bare)
            modify_command.append('--macaddress{}={}'.format(nic_index, mac))
        self._virtual_box.manage(modify_command)
        self._update()

    def setup_network_access(self, host_ports, guest_ports):
        """Set port forwarding using limited local ports range.

        :param host_ports: Range of local ports allowed to use.
        :param guest_ports: Mapping from `<protocol>/<port>` to index in
        host_ports. For example, TCP port 7001 has local counterpart
        `host_ports[guest_ports['tcp/7001']]`.
        """
        # Resetting port forwarding configuration may help when VirtualBox
        # doesn't open local port. (Reasons unclear.)
        # TODO: Reset NAT and port forwarding only in case of problems.
        _logger.debug("Remove existing port forwarding from VM %s.", self.name)
        for port in self._port_forwarding:
            self._manage_nic(1, 'natpf', 'delete', port.tag)
        if not guest_ports:
            _logger.warning("No ports are forwarded to VM %s.", self.name)
            return
        for (protocol, guest_port), hint in guest_ports.items():
            host_port = host_ports[hint]
            forwarded_port = _ForwardedPort.new(protocol, guest_port, host_port)
            self._manage_nic(1, 'natpf', forwarded_port.conf())
        self._update()

    def destroy(self):
        self.power_off(already_off_ok=True)
        self._virtual_box.manage(['unregistervm', self.name, '--delete'])
        _logger.info("Machine %s is destroyed.", self.name)

    def is_on(self):
        self._update()
        return self._is_running

    def is_off(self):
        return not self.is_on()

    def power_on(self, already_on_ok=False):
        _logger.info('Powering on %s', self)
        try:
            self._virtual_box.manage(['startvm', self.name, '--type', 'headless'])
        except virtual_box_error_cls('VBOX_E_INVALID_OBJECT_STATE'):
            if not already_on_ok:
                raise
            return
        wait_for_truthy(self.is_on, logger=_logger.getChild('wait'))

    def power_off(self, already_off_ok=False):
        _logger.info('Powering off %s', self)
        try:
            self._virtual_box.manage(['controlvm', self.name, 'poweroff'])
        except VirtualBoxError as e:
            if 'is not currently running' not in str(e):
                raise
            if not already_off_ok:
                raise
            return
        wait_for_truthy(self.is_off, logger=_logger.getChild('wait'))

    def plug_internal(self, network_name):
        slot = self._find_vacant_nic()
        self._manage_nic(slot, 'nic', 'intnet', network_name)
        return self.macs[slot]

    def plug_bridged(self, host_nic):
        slot = self._find_vacant_nic()
        self._manage_nic(slot, 'nic', 'bridged', host_nic)
        self._update()
        return self.macs[slot]

    def unplug_all(self):
        self._update()
        for nic_index in self.macs.keys():
            self._manage_nic(nic_index, 'nic', 'null')

    def limit_bandwidth(self, speed_limit_kbit):
        """See: https://www.virtualbox.org/manual/ch06.html#network_bandwidth_limit:

        > The limits for each group can be changed while the VM is running, with changes being
        picked up immediately. The example below changes the limit for the group created in the
        example above to 100 Kbit/s:
        ```{.sh}
        VBoxManage bandwidthctl "VM name" set Limit --limit 100k
        ```

        In `./configure-vm.sh`, each NIC gets its own bandwidth group, `network1`, `network2`...
        """
        for nic_index in [_HOST_CONNECTION_NIC_INDEX] + _INTERNAL_NIC_INDICES:
            self._virtual_box.manage([
                'bandwidthctl', self.name,
                'set', 'network{}'.format(nic_index),
                '--limit', '{}k'.format(speed_limit_kbit)])

    def reset_bandwidth(self):
        """See: https://www.virtualbox.org/manual/ch06.html#network_bandwidth_limit:

        > It is also possible to disable shaping for all adapters assigned to a bandwidth group
        while VM is running, by specifying the zero limit for the group. For example, for the
        bandwidth group named "Limit" use:
        ```{.sh}
        VBoxManage bandwidthctl "VM name" set Limit --limit 0
        ```
        """
        self.limit_bandwidth(0)

    def _manage_nic(self, nic_index, command, *arguments):
        if self._is_running:
            prefix = ['controlvm', self.name, command + str(nic_index)]
        else:
            prefix = ['modifyvm', self.name, '--' + command + str(nic_index)]
        return self._virtual_box.manage(prefix + list(arguments))

    def recovering(self, power_on_timeout_sec):
        """Between each yield, try something to bring VM up."""
        # Normal operation.
        if not self._is_running:
            _logger.debug("Recover %r: powered off. Power on.", self)
            self.power_on()
            _logger.debug("Recover %r: allow %d sec.", self, power_on_timeout_sec)
            yield power_on_timeout_sec
            _logger.warning("Recover %r: allow %d sec: first run?", self, power_on_timeout_sec)
            yield power_on_timeout_sec
        else:
            _logger.debug("Recover %r: powered on, expect response on first attempt.", self)
            yield 0
        _logger.warning("Recover %r: VirtualBox might not open ports, check it.", self)
        for port in self._port_forwarding:
            if not port_is_open(port.protocol, '127.0.0.1', port.host_port):
                self._manage_nic(1, 'natpf', 'delete', port.tag)
                self._manage_nic(1, 'natpf', port.conf())
        _logger.debug("Recover %r: allow 10 sec: setting up port forwarding.", self)
        yield 10
        _logger.warning("Recover %r: got \"Can't allocate mbuf\" in logs?", self)
        self._manage_nic(1, 'nic', 'null')
        self._manage_nic(1, 'nic', 'nat')
        _logger.debug("Recover %r: allow 30 sec: setting up network adapter.", self)
        yield 30
        _logger.warning("Recover %r: reason unknown, try reboot.", self)
        self.power_off()
        self.power_on()
        _logger.warning("Recover %r: allow %d sec: booting.", self, power_on_timeout_sec)
        yield power_on_timeout_sec
        raise VmUnresponsive("Recover %r: couldn't recover.".format(self))


class VirtualBox(Hypervisor):
    """Interface for VirtualBox as hypervisor."""

    ## Network for management. Host, as it's seen from VMs, is the part of this network and its
    # address, as seen from VMs, is in this network. Host has a special address in NAT network, it
    # is `(net & mask) + 2`. See: https://www.virtualbox.org/manual/ch09.html#idm8375.
    #
    # Network address is set in `./configure-vm.sh`.
    #
    # For connections that are initiated from VM,
    # other NIC is used to make it possible to control traffic by the means of VirtualBox.
    _nat_subnet = IPNetwork('192.168.253.0/24')

    def __init__(self, host_os_access, runner_address):
        """Create VirtualBox hypervisor.
        @param runner_address: Address of machine this process is running on as seen from machine
            VirtualBox is working on. If it's the same machine, and the address of the machine is
            referred to as a local loopback (127.0.0.1), the address has to be changed to the
            address, by which VMs can access the machine. Otherwise, the address should be
            accessible from VMs.
        """
        if runner_address.is_loopback():
            runner_address = self.__class__._nat_subnet.ip + 2
        super(VirtualBox, self).__init__(host_os_access, runner_address)

    def _get_file_path_for_import_vm(self, vm_image_path, timeout_sec):
        if vm_image_path.suffix == '.ova':
            try:
                # TODO: unpack command to run under Windows or, at least, check OS before un-tar
                stdout = self.host_os_access.run_command(
                    ['tar', 'xvf', vm_image_path, '-C', vm_image_path.parent],
                    timeout_sec=timeout_sec)
            # www.gnu.org/software/tar/manual/html_section/tar_19.html
            # Possible exit codes of GNU tar are summarized in the following table:
            #
            # 0 `Successful termination'.
            # 1 `Some files differ'. If tar was invoked with `--compare' (`--diff', `-d')
            #   command line option, this means that some files in the archive differ from
            #   their disk counterparts (see section Comparing Archive Members with the File System).
            #   If tar was given `--create', `--append' or `--update' option, this exit code means
            #   that some files were changed while being archived and so the resulting archive
            #   does not contain the exact copy of the file set.
            # 2 `Fatal error'. This means that some fatal, unrecoverable error occurred.
            except exit_status_error_cls(2) as x:
                stderr_decoded = x.stderr.decode('ascii')
                raise VirtualBoxError(
                    "Unpack image OVA archive '%s' error: %s" % (
                        vm_image_path, stderr_decoded))

            for line in stdout.strip().splitlines():
                if line.endswith('.ovf'):
                    return vm_image_path.parent / line
            raise VirtualBoxError(
                "Image OVA archive '%s' doesn't contain OVF file" % vm_image_path)

        return vm_image_path

    def import_vm(self, vm_image_path, vm_name):
        vm_import_file_path = self._get_file_path_for_import_vm(vm_image_path, timeout_sec=600)
        self.manage(['import', vm_import_file_path, '--vsys', 0, '--vmname', vm_name], timeout_sec=600)
        self.manage(['snapshot', vm_name, 'take', 'template'])
        # Group is assigned only when imported: cloned VMs are created nearby.
        group = '/' + vm_name.rsplit('-', 1)[0]
        try:
            self.manage(['modifyvm', vm_name, '--groups', group])
        except virtual_box_error_cls('NS_ERROR_INVALID_ARG') as e:
            _logger.error("Cannot assign group to VM %r: %s", vm_name, e)
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
        self.manage(['modifyvm', vm_name, '--description', 'dummy_user\ndummy_pass\ndummy_key'])
        self.manage(['modifyvm', vm_name, '--nic1', 'nat'])
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
            return self.host_os_access.run_command(
                ['VBoxManage'] + args, timeout_sec=timeout_sec, logger=_logger.getChild('manage'))
        except exit_status_error_cls(1) as x:
            stderr_decoded = x.stderr.decode('ascii')
            first_line = stderr_decoded.splitlines()[0]
            prefix = 'VBoxManage: error: '
            if not first_line.startswith(prefix):
                raise VirtualBoxError(stderr_decoded)
            message = first_line[len(prefix):]
            if message == "The object is not ready":
                raise VmNotReady("VBoxManage fails: {}".format(message))
            mo = re.search(r'Details: code (\w+)', stderr_decoded)
            if not mo:
                raise VirtualBoxError(message)
            code = mo.group(1)
            if code == 'VBOX_E_OBJECT_NOT_FOUND':
                mo = re.match(prefix + r"Could not find a registered machine named '(.+)'", first_line)
                if mo:
                    raise VMNotFound("Cannot find VM: {!r}\n{}".format(mo.group(1), message))
            if code == 'VBOX_E_FILE_ERROR' and 'already exists' in message:
                raise VMAlreadyExists(message)
            raise virtual_box_error_cls(code)(message)

    def make_internal_network(self, network_name):
        return '{} {} flat'.format(uuid1(), network_name)
