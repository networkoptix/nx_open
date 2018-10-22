import logging
from abc import ABCMeta, abstractmethod
from contextlib import contextmanager
from datetime import datetime

import six
from netaddr import IPAddress
from pathlib2 import PureWindowsPath
from typing import Callable, ContextManager, Mapping, Optional, Type

from framework.networking.interface import Networking
from framework.os_access.command import DEFAULT_RUN_TIMEOUT_SEC
from framework.os_access.exceptions import DoesNotExist
from framework.os_access.local_path import LocalPath
from framework.os_access.path import FileSystemPath
from framework.os_access.traffic_capture import TrafficCapture
from framework.threaded import ThreadedCall
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


@six.add_metaclass(ABCMeta)
class Time(object):
    @abstractmethod
    def get_tz(self):
        pass

    @abstractmethod
    def get(self):  # type: () -> RunningTime
        pass

    @abstractmethod
    def set(self, aware_datetime):  # type: (datetime) -> RunningTime
        pass


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

    @classmethod
    def port_forwarding(cls, forwarding_map, addr_forwarded_from, local_addr_for_remote):
        to_remote = OneWayPortMap.forwarding(forwarding_map, addr_forwarded_from)
        to_local_from_remote = OneWayPortMap.direct(local_addr_for_remote)
        cls(to_remote, to_local_from_remote)


class OSAccess(object):
    __metaclass__ = ABCMeta

    def __init__(
            self,
            alias,  # type: str
            port_map,  # type: ReciprocalPortMap
            networking,  # type: Optional[Networking]
            time,  # type: Time
            traffic_capture,  # type: Optional[TrafficCapture]
            lock_acquired,  # type: Optional[Callable[[FileSystemPath, ...], ContextManager[None]]]
            path_cls,  # type: Type[FileSystemPath]
            ):
        self.alias = alias
        self.port_map = port_map
        self.networking = networking
        self.traffic_capture = traffic_capture
        self.time = time
        self.lock_acquired = lock_acquired
        self.path_cls = path_cls

    @abstractmethod
    def run_command(self, command, input=None, logger=None, timeout_sec=DEFAULT_RUN_TIMEOUT_SEC):
        # type: (list, bytes, logging.Logger, int) -> bytes
        """For applications with cross-platform CLI"""
        return b'stdout'

    @abstractmethod
    def is_accessible(self):
        return False

    @abstractmethod
    def env_vars(self):
        return {'NAME': 'value'}  # Used as a type hint only.

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
        return self.path_cls()

    @abstractmethod
    def _fs_root(self):
        return self.path_cls()

    def _disk_space_holder(self):  # type: () -> FileSystemPath
        return self._fs_root() / 'space_holder.tmp'

    @abstractmethod
    def _free_disk_space_bytes_on_all(self):  # type: () -> Mapping[FileSystemPath, int]
        pass

    def free_disk_space_bytes(self):  # type: () -> int
        result_on_all = self._free_disk_space_bytes_on_all()
        return result_on_all[self._fs_root()]

    @abstractmethod
    def _hold_disk_space(self, to_consume_bytes):  # type: (int) -> None
        pass

    def cleanup_disk_space(self):
        try:
            self._disk_space_holder().unlink()
        except DoesNotExist:
            pass

    def _limit_free_disk_space(self, should_leave_bytes):  # type: (int) -> ...
        delta_bytes = self.free_disk_space_bytes() - should_leave_bytes
        if delta_bytes > 0:
            try:
                current_size = self._disk_space_holder().size()
            except DoesNotExist:
                current_size = 0
            total_bytes = delta_bytes + current_size
            self._hold_disk_space(total_bytes)

    @contextmanager
    def free_disk_space_limited(self, should_leave_bytes, interval_sec=1):
        # type: (int, float) -> ContextManager[...]
        """Try to maintain limited free disk space while in this context.

        One-time allocation (reservation) of disk space is not enough. OS or other software
        (Mediaserver) may free disk space by deletion of temporary files or archiving other files,
        especially as reaction on low free disk space, limiting of which is the point of this
        function. That's why disk space is reallocated one time per second in case some space is
        freed by OS or software. Hence the thread.

        Disk space is never given back while in this context as it makes the main point of
        free space limiting useless. Disk space is limited to make impossible for tested software
        create large file but, if disk space were given back, it would be possible to write file
        chunk by chunk.
        """
        self._limit_free_disk_space(should_leave_bytes)

        def target():
            self._limit_free_disk_space(should_leave_bytes)

        try:
            with ThreadedCall.periodic(target, interval_sec, interval_sec + 1):
                yield
        finally:
            self.cleanup_disk_space()

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
            return destination_dir.take_from(local_path)
        raise NotImplementedError("Unknown scheme: {}".format(source_url))

    @abstractmethod
    def _download_by_http(self, source_url, destination_dir, timeout_sec):
        return self.path_cls()

    @abstractmethod
    def _download_by_smb(self, source_hostname, source_path, destination_dir, timeout_sec):
        return self.path_cls()
