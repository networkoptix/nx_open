from abc import ABCMeta, abstractmethod, abstractproperty
from datetime import datetime

from netaddr import IPAddress

from framework.networking.interface import Networking
from framework.os_access.path import FileSystemPath
from framework.utils import RunningTime


class _AllPorts(object):
    def __getitem__(self, protocol_port):
        protocol, port = protocol_port
        if protocol not in {'tcp', 'udp'}:
            raise KeyError("Protocol must be either 'tcp' or 'udp' but given {!r}".format(protocol))
        if not 1 <= port <= 65535:
            raise KeyError("Port must be an int between {!r} and {!r} but given {!r}".format(1, 65535, port))
        return port


class OneWayPortMap(object):
    """Map protocol and port to address and port, protocol is same

    >>> to_remote = OneWayPortMap.forwarding({('tcp', 10): 20, ('udp', 30): 40})
    >>> to_local = OneWayPortMap.direct(IPAddress('254.0.0.1'))
    >>> str(to_remote.address), to_remote.tcp(10), to_remote.udp(30)
    ('127.0.0.1', 20, 40)
    >>> str(to_local.address), to_local.tcp(10), to_local.udp(30)
    ('254.0.0.1', 10, 30)
    """

    def __init__(self, address, forwarded_ports_dict):
        self.address = address
        self._forwarded_ports_dict = forwarded_ports_dict

    @classmethod
    def direct(cls, address):
        return cls(address, _AllPorts())

    @classmethod
    def local(cls):
        return cls.direct(IPAddress('127.0.0.1'))

    @classmethod
    def forwarding(cls, protocol_forwarded_port_to_port, address_forwarded_from=IPAddress('127.0.0.1')):
        return cls(address_forwarded_from, protocol_forwarded_port_to_port)

    def tcp(self, port):
        """Shortcut"""
        forwarded_port = self._forwarded_ports_dict['tcp', port]  # type: int
        return forwarded_port

    def udp(self, port):
        """Shortcut"""
        forwarded_port = self._forwarded_ports_dict['udp', port]  # type: int
        return forwarded_port


class ReciprocalPortMap(object):
    """Single class to describe how local and remote machines can reach together

    Local is machine this code is running on.
    Remote is machine this code access.
    This object comes either from remote physical machine configuration of from hypervisor.
    Interface is symmetric.
    """

    def __init__(self, remote, local):
        # Remote port map. Knows how to access ports on remote machine from local.
        self.remote = remote  # type: OneWayPortMap
        # Local port map. Knows how to access ports on local machine from remote.
        self.local = local  # type: OneWayPortMap


class OSAccess(object):
    __metaclass__ = ABCMeta

    @abstractmethod
    def run_command(self, command, input=None):  # type: (list, bytes) -> bytes
        """For applications with cross-platform CLI"""
        return b'stdout'

    @abstractmethod
    def is_accessible(self):
        return False

    # noinspection PyPep8Naming
    @abstractproperty
    def Path(self):
        return FileSystemPath

    @abstractproperty
    def networking(self):
        return Networking()

    @abstractproperty
    def port_map(self):
        return ReciprocalPortMap(OneWayPortMap.local(), OneWayPortMap.local())

    @abstractmethod
    def get_time(self):
        pass

    @abstractmethod
    def set_time(self, new_time):  # type: (datetime.datetime) -> RunningTime
        return RunningTime(datetime.now())

    @abstractmethod
    def make_core_dump(self, pid):
        pass

    @abstractmethod
    def make_fake_disk(self, name, size_bytes):
        return self.Path()
