import csv
import logging
from collections import namedtuple
from pprint import pformat

from decorator import contextmanager
from pylru import lrudecorator

from test_utils.move_lock import MoveLock
from test_utils.networking import NodeNetworking, reset_networking
from test_utils.networking.linux import LinuxNodeNetworking
from test_utils.networking.virtual_box import VirtualBoxNodeNetworking
from test_utils.os_access import FileNotFound, ProcessError
from test_utils.serialize import load, dump
from test_utils.utils import wait_until

logger = logging.getLogger(__name__)


class VMNotFound(Exception):
    pass


class VMConfiguration(object):
    class HostPortOffsetOutOfRange(Exception):
        pass

    def __init__(self, raw_dict):
        self._raw_dict = raw_dict
        self.template = self._raw_dict['template']  # Type may be specific for hypervisor.
        self.power_on_timeout = float(self._raw_dict['power_on_timeout'])

    def name(self, index):
        return self._raw_dict['name_prefix'] + '{:03d}'.format(index)

    def forwarded_ports(self, index):
        configuration = self._raw_dict['port_forwarding']
        for name, forwarded_port in configuration['forwarded_ports'].items():
            host_port_base = configuration['host_ports_base'] + index * configuration['host_ports_per_vm']
            host_port = host_port_base + forwarded_port['host_port_offset']
            if not forwarded_port['host_port_offset'] < configuration['host_ports_per_vm']:
                raise self.HostPortOffsetOutOfRange("Configuration:\n{}", pformat(configuration))
            yield name, forwarded_port['protocol'], forwarded_port['guest_port'], host_port


class VirtualBox(object):
    """Interface for VirtualBox as hypervisor."""

    class VMInfo(object):
        def __init__(self, hypervisor_address, raw_dict):
            self._hypervisor_address = hypervisor_address
            self._raw_dict = raw_dict
            self.name = self._raw_dict['name']
            self.is_running = self._raw_dict['VMState'] == 'running'
            self.ports = {}
            for index in range(100):
                try:
                    raw_value = self._raw_dict['Forwarding({})'.format(index)]
                except KeyError:
                    break
                tag, protocol, host_address, host_port, guest_address, guest_port = raw_value.split(',')
                self.ports[protocol, int(guest_port)] = hypervisor_address, int(host_port)

    def __init__(self, os_access, address):
        self._os_access = os_access
        self._address = address

    def _info(self, name):
        try:
            output = self._os_access.run_command(['VBoxManage', 'showvminfo', name, '--machinereadable'])
        except ProcessError as e:
            if e.returncode == 1:
                raise VMNotFound("Cannot find VM {}; VBoxManage says:\n{}".format(name, e.output))
            raise
        raw_dict = dict(csv.reader(output.splitlines(), delimiter='=', escapechar='\\', doublequote=False))
        info = self.VMInfo(self._address, raw_dict)
        return info

    def find(self, name):
        return self._info(name)

    def power_on(self, name):
        self._os_access.run_command(['VBoxManage', 'startvm', name, '--type', 'headless'])

    def power_off(self, name):
        self._os_access.run_command(['VBoxManage', 'controlvm', name, 'acpipowerbutton'])

    def create(self, name, template, forwarded_ports):
        """Clone and setup VM with simple and generic parameters. Template can be any specific type."""
        self._os_access.run_command([
            'VBoxManage', 'clonevm', template['vm'],
            '--snapshot', template['snapshot'],
            '--name', name,
            '--options', 'link',
            '--register'])
        for tag, protocol, guest_port, host_port in forwarded_ports:
            self._os_access.run_command([
                'VBoxManage', 'modifyvm', name,
                '--natpf1', '{},{},,{},,{}'.format(tag, protocol, host_port, guest_port)])
        return self._info(name)

    def destroy(self, name):
        if self.find(name).is_running:
            self._os_access.run_command(['VBoxManage', 'controlvm', name, 'poweroff'])
            assert wait_until(lambda: not self.find(name).is_running, name='until VM is off')
        self._os_access.run_command(['VBoxManage', 'unregistervm', name, '--delete'])


Machine = namedtuple('Machine', ['alias', 'index', 'name', 'ports', 'os_access', 'networking'])


class Registry(object):
    """Manage names allocation. Safe for parallel usage."""

    class NothingAvailable(Exception):
        pass

    def __init__(self, os_access, base_dir, limit):
        self._os_access = os_access
        self._path = base_dir / 'registry.yaml'
        self._limit = limit
        self._lock = MoveLock(self._os_access, self._path.with_suffix('.lock'))
        if not self._os_access.dir_exists(self._path.parent):
            logger.warning("Create %r; OK only if a clean run", self._path.parent)
            self._os_access.mk_dir(self._path.parent)

    def _read_reservations(self):
        try:
            contents = self._os_access.read_file(self._path)
        except FileNotFound:
            return {}
        else:
            return load(contents)

    def reserve(self, mark, make_name):
        with self._lock:
            for index in range(self._limit):
                name = make_name(index)
                reservations = self._read_reservations()
                if name not in reservations or reservations[name] is None:
                    reservations[name] = mark
                    self._os_access.write_file(self._path, dump(reservations))
                    return index, name
            raise self.NothingAvailable()

    def relinquish(self, name):
        with self._lock:
            reservations = self._read_reservations()
            reservations[name] = None
            self._os_access.write_file(self._path, dump(reservations))

    @contextmanager
    def clean(self):
        with self._lock:
            reservations = self._read_reservations()
            yield reservations.keys()
            self._os_access.rm_tree(self._path, ignore_errors=True)  # Not removed in case of exception.


class MachineNotResponding(Exception):
    def __init__(self, alias, name):
        super(MachineNotResponding, self).__init__("Machine {} ({}) is not responding".format(name, alias))
        self.alias = alias
        self.name = name


class Pool(object):
    """Get (with caching), allocate (bypassing cache) and recycle VMs."""

    def __init__(self, vm_configuration, registry, hypervisor, access_manager):
        self._vm_configuration = vm_configuration
        self._access_manager = access_manager
        self._registry = registry
        self._hypervisor = hypervisor

    def allocate(self, alias):
        index, name = self._registry.reserve(alias, self._vm_configuration.name)
        try:
            info = self._hypervisor.find(name)
        except VMNotFound:
            forwarded_ports = self._vm_configuration.forwarded_ports(index)
            info = self._hypervisor.create(name, self._vm_configuration.template, forwarded_ports)
        assert info.name == name
        if not info.is_running:
            self._hypervisor.power_on(info.name)
        hostname, port = info.ports['tcp', self._access_manager.guest_port]
        os_access = self._access_manager.register(hostname, [alias, info.name], port)
        if not wait_until(os_access.is_working, timeout_sec=self._vm_configuration.power_on_timeout):
            raise MachineNotResponding(alias, info.name)
        networking = NodeNetworking(LinuxNodeNetworking(os_access), VirtualBoxNodeNetworking(info.name))
        machine = Machine(alias, index, info.name, info.ports, os_access, networking)
        reset_networking(machine)
        return machine

    @lrudecorator(100)
    def get(self, alias):
        return self.allocate(alias)

    def recycle(self, machine):
        del self.get.cache[(self, machine.alias), ()]
        self._registry.relinquish(machine.name)

    def close(self):
        for key in self.get.cache:
            machine = self.get.cache[key]
            (pool, alias), empty_kwargs = key
            if pool is self:
                assert not empty_kwargs
                assert alias == machine.alias
                self.recycle(machine)

    def clean(self):
        with self._registry.clean():
            pass

    def destroy(self):
        with self._registry.clean() as names:
            for name in names:
                try:
                    self._hypervisor.find(name)
                except VMNotFound:
                    logger.warning("Machine %s is not created yet", name)
                else:
                    self._hypervisor.destroy(name)
