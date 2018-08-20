import logging
import select
import socket
import time
from abc import ABCMeta
from contextlib import contextmanager, closing
from io import StringIO

import paramiko

from framework.method_caching import cached_getter
from framework.os_access.command import Command, Run
from framework.os_access.local_path import LocalPath
from framework.os_access.path import copy_file_using_read_and_write
from framework.os_access.posix_shell import PosixOutcome, PosixShell, _STREAM_BUFFER_SIZE
from framework.os_access.posix_shell_path import PosixShellPath
from framework.os_access.posix_shell_utils import sh_augment_script, sh_command_to_script

_logger = logging.getLogger(__name__)


class SSHNotConnected(Exception):
    pass


class _SSHCommandOutcome(PosixOutcome):
    def __init__(self, exit_status):
        assert isinstance(exit_status, int)
        assert 0 <= exit_status <= 255 or exit_status == -1
        self._exit_status = exit_status

    @property
    def signal(self):
        if 128 < self._exit_status < 128 + 32:
            return self._exit_status - 128
        return None

    @property
    def code(self):
        if self._exit_status == -1:
            return None
        return self._exit_status

    @property
    def comment(self):
        if self._exit_status == -1:
            return "Unknown exit status: server hasn't sent it"
        return super(_SSHCommandOutcome, self).comment


class _SSHRun(Run):
    __metaclass__ = ABCMeta

    def __init__(self, channel):  # type: (paramiko.Channel) -> None
        super(_SSHRun, self).__init__()
        self._channel = channel

    @property
    def outcome(self):
        if not self._channel.exit_status_ready():
            return None
        return _SSHCommandOutcome(self._channel.exit_status)

    def receive(self, timeout_sec):
        self._channel.setblocking(False)
        # TODO: Check on Windows. Paramiko uses sockets on Windows as prescribed in Python docs.
        # TODO: Wait for exit status too.
        try:
            select.select([self._channel], [], [], timeout_sec)
        except ValueError as e:
            if e.message == "filedescriptor out of range in select()":
                raise RuntimeError("Limit of file descriptors are reached; use `ulimit -n`")
            raise
        chunks = []
        for recv in [self._channel.recv, self._channel.recv_stderr]:
            try:
                chunk = recv(_STREAM_BUFFER_SIZE)
            except socket.timeout:  # Non-blocking: times out immediately if no data.
                chunks.append(b'')
            else:
                stream_is_closed = len(chunk) == 0  # Exactly as said in its docstring.
                if stream_is_closed:
                    chunks.append(None)
                else:
                    chunks.append(chunk)
        return chunks


class _PseudoTerminalSSHRun(_SSHRun):
    _logger = _logger.getChild('_PseudoTerminalSSHRun')

    def __init__(self, channel, script):
        super(_PseudoTerminalSSHRun, self).__init__(channel)
        self._channel.get_pty()
        self._channel.invoke_shell()
        self._logger.debug("Run: %s", script)
        self._channel.send(script)
        self._channel.send('\n')
        self._open_streams = {
            "STDOUT": (self._channel.recv, self._logger.getChild('stdout')),
            "STDERR": (self._channel.recv_stderr, self._logger.getChild('stderr')),
            }

    def send(self, input, is_last=False):
        raise NotImplementedError(
            "Sending data to pseudo-terminal is prohibited intentionally; "
            "it's equivalent to pressing keys on keyboard, "
            "therefore, there is no straightforward way to pass binary data; "
            "on other hand, interaction with terminal is not needed by now")

    def terminate(self):
        """Send to terminal ASCII End-of-Text (code 3) character, equivalent to Ctrl+C

        See: https://stackoverflow.com/a/6196328/1833960.
        This way of terminating process is the whole point of this class.
        """
        self._channel.send('\x03')
        time.sleep(0.1)
        self._channel.send('\x04')


class _StraightforwardSSHRun(_SSHRun):
    _logger = _logger.getChild('_StraightforwardSSHRun')

    def __init__(self, channel, script):
        super(_StraightforwardSSHRun, self).__init__(channel)
        self._logger.debug("Run on %r:\n%s", self, script)
        self._channel.exec_command(script)
        self._open_streams = {
            "STDOUT": (self._channel.recv, self._logger.getChild('stdout')),
            "STDERR": (self._channel.recv_stderr, self._logger.getChild('stderr')),
            }

    def send(self, input, is_last=False):
        self._channel.settimeout(10)  # Must never time out assuming process always open stdin and read from it.
        input = memoryview(input)
        if input:
            bytes_sent = self._channel.send(input[:_STREAM_BUFFER_SIZE])
            _logger.debug("Chunk of input sent: %d bytes.", bytes_sent)
            if bytes_sent == 0:
                _logger.warning("Write direction preliminary closed from the other side.")
                # TODO: Raise exception.
        else:
            bytes_sent = 0
        if is_last and not input[bytes_sent:]:
            _logger.debug("Shutdown write direction.")
            self._channel.shutdown_write()
        return bytes_sent

    def terminate(self):
        raise NotImplementedError(
            "SSH, as a protocol, cannot send signals or terminate process in any other way; "
            "use pseudo-terminal, which can send Ctrl+C, but it has its own restrictions")


class _SSHCommand(Command):
    __metaclass__ = ABCMeta

    def __init__(self, ssh_client, script, terminal=False):  # type: (paramiko.SSHClient, str, bool) -> None
        self._ssh_client = ssh_client
        self._script = script
        self._terminal = terminal

    @contextmanager
    def running(self):
        with self._ssh_client.get_transport().open_session() as channel:
            if self._terminal:
                yield _PseudoTerminalSSHRun(channel, self._script)
            else:
                yield _StraightforwardSSHRun(channel, self._script)


class SSH(PosixShell):
    def __init__(self, hostname, port, username, key):
        self._hostname = hostname
        self._port = port
        self._username = username
        self._key = key

    def __repr__(self):
        return '<{!s}>'.format(sh_command_to_script([
            'ssh', '{!s}@{!s}'.format(self._username, self._hostname),
            '-p', self._port,
            ]))

    def command(self, args, cwd=None, env=None, set_eux=True):
        script = sh_command_to_script(args)
        return self.sh_script(script, cwd=cwd, env=env, set_eux=set_eux)

    def terminal_command(self, args, cwd=None, env=None):
        script = sh_augment_script(sh_command_to_script(args), cwd=cwd, env=env, set_eux=False, shebang=False)
        return _SSHCommand(self._client(), script, terminal=True)

    @cached_getter
    def _client(self):
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.MissingHostKeyPolicy())  # Ignore completely.
        try:
            client.connect(
                str(self._hostname), port=self._port,
                username=self._username, pkey=paramiko.RSAKey.from_private_key(StringIO(self._key)),
                look_for_keys=False, allow_agent=False)
        except paramiko.ssh_exception.NoValidConnectionsError as e:
            raise SSHNotConnected("Cannot connect to {}: {} (is port opened?)".format(self, e))
        except paramiko.ssh_exception.SSHException as e:
            raise SSHNotConnected("Cannot connect to {}: {} (is service started? using VirtualBox?)".format(self, e))
        return client

    def close(self):
        self._client().close()

    def __del__(self):
        self.close()

    def sh_script(self, script, cwd=None, env=None, set_eux=True):
        augmented_script = sh_augment_script(script, cwd=cwd, env=env, set_eux=set_eux)
        return _SSHCommand(self._client(), augmented_script)

    def copy_posix_file_to(self, posix_source, destination):
        assert isinstance(posix_source, PosixShellPath), repr(posix_source)
        assert isinstance(posix_source._shell, SSH), repr(posix_source._shell)
        if isinstance(destination, LocalPath):
            with closing(self._client().open_sftp()) as sftp:
                sftp.get(str(posix_source), str(destination))
        else:
            copy_file_using_read_and_write(posix_source, destination)

    def copy_file_from_posix(self, source, posix_destination):
        assert isinstance(posix_destination, PosixShellPath), repr(posix_destination)
        assert isinstance(posix_destination._shell, SSH), repr(posix_destination._shell)
        if isinstance(source, LocalPath):
            with closing(self._client().open_sftp()) as sftp:
                sftp.put(str(source), str(posix_destination))
        else:
            copy_file_using_read_and_write(source, posix_destination)

    def is_working(self):
        try:
            self.run_sh_script(':')
        except SSHNotConnected:
            return False
        return True
