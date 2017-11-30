import contextlib
import datetime
import socket
import struct

import pytz

from test_utils.host import ProcessError
from test_utils.utils import wait_until, RunningTime

RFC868_SERVER_ADDRESS = 'time.rfc868server.com'
RFC868_SERVER_PORT = 37
RFC868_SERVER_TIMEOUT_SEC = 3  # Timeout for time server response, seconds

_INTERNET_TIME_CACHE = None


def get_internet_time():
    global _INTERNET_TIME_CACHE
    if _INTERNET_TIME_CACHE is None:
        _INTERNET_TIME_CACHE = _request_internet_time_server()
    return _INTERNET_TIME_CACHE


class TimeProtocolRestriction(object):
    def __init__(self, server):
        self.server = server

    def _run_iptables_command(self, iptables_rule_command):
        self.server.host.run_command([
            'iptables', iptables_rule_command, 'OUTPUT',
            '--protocol', 'tcp', '--dport', '37',
            '--jump', 'REJECT'])

    def _can_connect_to_time_server(self):
        command = ['nc', '-w', str(RFC868_SERVER_TIMEOUT_SEC), RFC868_SERVER_ADDRESS, str(RFC868_SERVER_PORT)]
        try:
            self.server.host.run_command(command)
        except ProcessError as e:
            assert e.returncode == 1, "Unexpected return code when checking connection to internet time server."
            return False
        else:
            return True

    def enable(self):
        try:
            self._run_iptables_command('--check')
        except ProcessError as e:
            assert e.returncode == 1, "Unexpected return code when checking if iptables rule exists."
            self._run_iptables_command('--append')
        assert wait_until(lambda: not self._can_connect_to_time_server()), (
            "Connected to internet time server while iptables rule exists.")

    def disable(self):
        while True:
            try:
                self._run_iptables_command('--delete')
            except ProcessError as e:
                assert e.returncode == 1, "Unexpected return code when deleting non-existent iptables rule."
                break
        assert wait_until(lambda: self._can_connect_to_time_server()), (
            "Cannot connect to internet time server while iptables rule doesn't exist.")


def _request_internet_time_server(address=RFC868_SERVER_ADDRESS, port=RFC868_SERVER_PORT):
    """Get time from RFC868 time server wrap into Python's datetime.
    >>> _request_internet_time_server()  # doctest: +ELLIPSIS
    RunningTime(...)
    """
    with contextlib.closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        started_at = datetime.datetime.now(pytz.utc)
        s.connect((address, port))
        time_data = s.recv(4)
        request_duration = datetime.datetime.now(pytz.utc) - started_at
    remote_as_rfc868_timestamp, = struct.unpack('!I', time_data)
    posix_to_rfc868_diff = datetime.datetime.fromtimestamp(0, pytz.utc) - datetime.datetime(1900, 1, 1, tzinfo=pytz.utc)
    remote_as_posix_timestamp = remote_as_rfc868_timestamp - posix_to_rfc868_diff.total_seconds()
    remote_as_datetime = datetime.datetime.fromtimestamp(remote_as_posix_timestamp, pytz.utc)
    return RunningTime(remote_as_datetime, request_duration)
