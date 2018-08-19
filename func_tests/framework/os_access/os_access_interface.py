import logging
from abc import ABCMeta, abstractmethod, abstractproperty
from datetime import datetime

from netaddr import IPAddress
from pathlib2 import PureWindowsPath

from framework.networking.interface import Networking
from framework.os_access.command import DEFAULT_RUN_TIMEOUT_SEC
from framework.os_access.local_path import LocalPath
from framework.os_access.path import FileSystemPath
from framework.os_access.traffic_capture import TrafficCapture
from framework.utils import RunningTime

_DEFAULT_DOWNLOAD_TIMEOUT_SEC = 30 * 60

_logger = logging.getLogger(__name__)


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
    def empty(cls):
        return cls(None, {})

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
    This object comes either from remote physical machine configuration or from hypervisor.
    Interface is symmetric.
    """

    def __init__(self, remote, local):
        # Remote port map. Knows how to access ports on remote machine from local.
        self.remote = remote  # type: OneWayPortMap
        # Local port map. Knows how to access ports on local machine from remote.
        self.local = local  # type: OneWayPortMap


class OSAccess(object):
    __metaclass__ = ABCMeta

    @abstractproperty
    def alias(self):
        pass

    @abstractmethod
    def run_command(self, command, input=None, timeout_sec=DEFAULT_RUN_TIMEOUT_SEC):  # type: (list, bytes, int) -> bytes
        """For applications with cross-platform CLI"""
        return b'stdout'

    @abstractmethod
    def is_accessible(self):
        return False

    @abstractmethod
    def env_vars(self):
        return {'NAME': 'value'}  # Used as a type hint only.

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
        # TODO: Find and return path.
        pass

    @abstractmethod
    def parse_core_dump(self, path, **options):
        """Parse process dump with OS-specific options"""
        # TODO: Decide on placement of this method. Where should it reside given that arguments differ?
        pass

    @abstractmethod
    def make_fake_disk(self, name, size_bytes):
        return self.Path()

    def download(self, source_url, destination_dir, timeout_sec=_DEFAULT_DOWNLOAD_TIMEOUT_SEC):
        _logger.info("Download %s to %r.", source_url, destination_dir)
        if source_url.startswith('http://') or source_url.startswith('https://'):
            return self._download_by_http(source_url, destination_dir, timeout_sec)
        if source_url.startswith('smb://'):
            hostname, path_str = source_url[len('smb://'):].split('/', 1)
            path = PureWindowsPath(path_str)
            return self._download_by_smb(hostname, path, destination_dir, timeout_sec)
        if source_url.startswith('file://'):
            local_path = LocalPath(source_url[len('file://'):])
            return self._take_local(local_path, destination_dir)
        raise NotImplementedError("Unknown scheme: {}".format(source_url))

    @abstractmethod
    def _download_by_http(self, source_url, destination_dir, timeout_sec):
        return self.Path()

    @abstractmethod
    def _download_by_smb(self, source_hostname, source_path, destination_dir, timeout_sec):
        return self.Path()

    @abstractmethod
    def _take_local(self, local_source_path, dir):
        return self.Path()

    @abstractmethod
    def lock(self, path):
        pass

    @abstractproperty
    def traffic_capture(self):
        return TrafficCapture(self.Path.tmp())