import logging
from collections import namedtuple
from pprint import pformat

from decorator import contextmanager
from pylru import lrudecorator

from test_utils.move_lock import MoveLock
from test_utils.networking.linux import LinuxNetworking
from test_utils.os_access import FileNotFound
from test_utils.serialize import dump, load
from test_utils.utils import wait_until
from test_utils.virtual_box import VMNotFound

logger = logging.getLogger(__name__)


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
            yield name, forwarded_port['protocol'], host_port, forwarded_port['guest_port']


VM = namedtuple('VM', ['alias', 'name', 'ports', 'networking', 'os_access'])


class Registry(object):
    """Manage names allocation. Safe for parallel usage."""

    class Error(Exception):
        def __init__(self, reservations, message):
            super(Exception, self).__init__("{}; current reservations:\n{}".format(message, pformat(reservations)))
            self.reservations = reservations

    class NoAvailableNames(Exception):
        def __init__(self, reservations, alias):
            super(Registry.NoAvailableNames, self).__init__(reservations, "No available name for {}".format(alias))
            self.alias = alias

    class NotReserved(Exception):
        def __init__(self, reservations, name):
            super(Registry.NotReserved, self).__init__(reservations, "Name {} is not reserved".format(name))
            self.name = name

    def __init__(self, os_access, path, limit, name_prefix):
        self._os_access = os_access
        self._path = path
        self._limit = limit
        self._lock = MoveLock(self._os_access, self._path.with_suffix('.lock'))
        if not self._os_access.dir_exists(self._path.parent):
            logger.warning("Create %r; OK only if a clean run", self._path.parent)
            self._os_access.mk_dir(self._path.parent)
        self._name_prefix = name_prefix

    def _read_reservations(self):
        try:
            contents = self._os_access.read_file(self._path)
        except FileNotFound:
            return {}
        else:
            return load(contents)

    def reserve(self, alias):
        with self._lock:
            reservations = self._read_reservations()
            for index in range(self._limit):
                name = '{}{:03d}'.format(self._name_prefix, index)
                if name not in reservations or reservations[name] is None:
                    reservations[name] = alias
                    self._os_access.write_file(self._path, dump(reservations))
                    return index, name
        raise self.NoAvailableNames(reservations, alias)

    def relinquish(self, name):
        with self._lock:
            reservations = self._read_reservations()
            if name in reservations and reservations[name] is not None:
                reservations[name] = None
                self._os_access.write_file(self._path, dump(reservations))
            else:
                raise self.NotReserved(reservations, name)

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
        index, name = self._registry.reserve(alias)
        try:
            info = self._hypervisor.find(name)
        except VMNotFound:
            forwarded_ports = self._vm_configuration.forwarded_ports(index)
            info = self._hypervisor.clone(name, self._vm_configuration.template, forwarded_ports)
        assert info.name == name
        if not info.is_running:
            self._hypervisor.power_on(info.name)
        hostname, port = info.ports['tcp', self._access_manager.guest_port]
        os_access = self._access_manager.register(hostname, [alias, info.name], port)
        if not wait_until(os_access.is_working, timeout_sec=self._vm_configuration.power_on_timeout):
            raise MachineNotResponding(alias, info.name)
        networking = LinuxNetworking(os_access, info.macs.values())
        vm = VM(alias, info.name, info.ports, networking, os_access)
        self._hypervisor.unplug_all(vm.name)
        vm.networking.reset()
        vm.networking.enable_internet()
        return vm

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
                    logger.warning("VM %s is not created yet", name)
                else:
                    self._hypervisor.destroy(name)
