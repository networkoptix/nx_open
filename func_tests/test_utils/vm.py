import logging
from collections import namedtuple
from pprint import pformat

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


VM = namedtuple('VM', ['alias', 'index', 'name', 'ports', 'networking', 'os_access'])


class Registry(object):
    """Manage names allocation. Safe for parallel usage."""

    class Error(Exception):
        def __init__(self, reservations, message):
            super(Exception, self).__init__("current reservations:\n{}".format(message, pformat(reservations)))
            self.reservations = reservations

    class NotReserved(Exception):
        def __init__(self, reservations, index):
            super(Registry.NotReserved, self).__init__(reservations, "Index {} is not reserved".format(index))
            self.index = index

    def __init__(self, os_access, path):
        self._os_access = os_access
        self._path = path
        self._lock = MoveLock(self._os_access, self._path.with_suffix('.lock'))
        if not self._os_access.dir_exists(self._path.parent):
            logger.warning("Create %r; OK only if a clean run", self._path.parent)
            self._os_access.mk_dir(self._path.parent)

    def __repr__(self):
        return '<Registry {}>'.format(self._path)

    def _read_reservations(self):
        try:
            contents = self._os_access.read_file(self._path)
        except FileNotFound:
            return []
        return load(contents)

    def reserve(self, alias):
        with self._lock:
            reservations = self._read_reservations()
            try:
                index = reservations.index(None)
            except ValueError:
                index = len(reservations)
                reservations.append(alias)
            else:
                reservations[index] = alias
            self._os_access.write_file(self._path, dump(reservations))
            logger.info("Index %.03d taken in %r.", index, self)
            return index

    def relinquish(self, index):
        with self._lock:
            reservations = self._read_reservations()
            try:
                alias = reservations[index]
            except IndexError:
                alias = None
            if alias is not None:
                reservations[index] = None
                self._os_access.write_file(self._path, dump(reservations))
                logger.info("Index %.03d freed in %r.", index, self)
            else:
                raise self.NotReserved(reservations, index)

    def for_each(self, procedure):
        with self._lock:
            reservations = self._read_reservations()
            for index, alias in enumerate(reservations):
                procedure(index, alias)


class MachineNotResponding(Exception):
    def __init__(self, alias, name):
        super(MachineNotResponding, self).__init__("Machine {} ({}) is not responding".format(name, alias))
        self.alias = alias
        self.name = name


class Factory(object):
    def __init__(self, vm_configuration, hypervisor, access_manager, registry):
        self._vm_configuration = vm_configuration
        self._access_manager = access_manager
        self._hypervisor = hypervisor
        self._registry = registry

    def allocate(self, alias):
        index = self._registry.reserve(alias)
        name = self._vm_configuration.name(index)
        try:
            info = self._hypervisor.find(name)
        except VMNotFound:
            forwarded_ports = self._vm_configuration.forwarded_ports(index)
            info = self._hypervisor.clone(name, index, self._vm_configuration.template, forwarded_ports)
        assert info.name == name
        if not info.is_running:
            self._hypervisor.power_on(info.name)
        hostname, port = info.ports['tcp', self._access_manager.guest_port]
        os_access = self._access_manager.register(alias, hostname, port)
        if not wait_until(
                os_access.is_working,
                name='until {} ({}) can be accesses via {!r}'.format(alias, name, os_access),
                timeout_sec=self._vm_configuration.power_on_timeout):
            raise MachineNotResponding(alias, info.name)
        networking = LinuxNetworking(os_access, info.macs.values())
        vm = VM(alias, index, info.name, info.ports, networking, os_access)
        self._hypervisor.unplug_all(vm.name)
        vm.networking.reset()
        vm.networking.enable_internet()
        return vm

    def release(self, vm):
        self._access_manager.unregister(vm.alias)
        self._registry.relinquish(vm.index)

    def cleanup(self):
        def destroy(index, alias):
            name = self._vm_configuration.name(index)
            if alias is None:
                try:
                    self._hypervisor.destroy(name)
                except VMNotFound:
                    logger.info("VM %s not found.", name)
                else:
                    logger.info("VM %s destroyed.", name)
            else:
                logger.warning("VM %s reserved now.", name)

        self._registry.for_each(destroy)
