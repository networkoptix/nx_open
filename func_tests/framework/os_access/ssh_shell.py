import logging
import select
import socket
from abc import ABCMeta

import paramiko

from framework.method_caching import cached_getter
from framework.os_access.command import Command
from framework.os_access.posix_shell import PosixOutcome, PosixShell, _BIG_CHUNK_THRESHOLD_CHARS, _STREAM_BUFFER_SIZE
from framework.os_access.posix_shell_utils import sh_augment_script, sh_command_to_script

_logger = logging.getLogger(__name__)


class SSHNotConnected(Exception):
    pass


class _SSHCommandOutcome(PosixOutcome):
    def __init__(self, exit_status):
        assert isinstance(exit_status, int)
        assert 0 <= exit_status <= 255
        self._exit_status = exit_status

    @property
    def signal(self):
        if 128 < self._exit_status < 128 + 32:
            return self._exit_status - 128
        return None

    @property
    def code(self):
        return self._exit_status


class _SSHCommand(Command):
    __metaclass__ = ABCMeta

    _logger = _logger.getChild('_SSHCommand')

    def __init__(self, ssh_client, script):
        super(_SSHCommand, self).__init__()
        self._ssh_client = ssh_client
        self._script = script
        self._channel = None  # type: paramiko.Channel
        self._open_streams = None  # type: dict

    def __exit__(self, exc_type, exc_val, exc_tb):
        pass

    def _receive_output(self, timeout_sec):
        self._channel.setblocking(False)
        output_chunks = {}
        # TODO: Check on Windows. Paramiko uses sockets on Windows as prescribed in Python docs.
        select.select([self._channel], [], [], timeout_sec)
        for key, (recv, output_logger) in list(self._open_streams.items()):  # Copy dict items.
            try:
                output_chunk = recv(_STREAM_BUFFER_SIZE)
            except socket.timeout:
                output_logger.debug("No data but not closed.")
                output_chunks[key] = b''
                continue  # Next stream might have data on it.
            if not output_chunk:
                output_logger.debug("Closed from the other side.")
                output_chunks[key] = None
                del self._open_streams[key]
                continue
            output_logger.debug("Received: %d bytes.", len(output_chunk))
            output_chunks[key] = output_chunk
            if len(output_chunk) > _BIG_CHUNK_THRESHOLD_CHARS:
                output_logger.debug("Big chunk: %d bytes", len(output_chunk))
            else:
                try:
                    output_logger.debug("Text:\n%s", output_chunk.decode('ascii'))
                except UnicodeDecodeError:
                    output_logger.debug("Binary: %d bytes", len(output_chunk))
        return output_chunks.get('STDOUT'), output_chunks.get('STDERR')

    def receive(self, timeout_sec):
        if not self._open_streams:
            self._channel.status_event.wait(timeout_sec)
            stdout, stderr = None, None
        else:
            stdout, stderr = self._receive_output(timeout_sec)
        self._logger.debug("Exit status: %r.", self._channel.exit_status)
        if self._channel.exit_status != -1:
            assert 0 <= self._channel.exit_status <= 255
            if self._open_streams:
                self._logger.error("Remote process exited but streams left open. Child forked?")
            exit_status = self._channel.exit_status
        else:
            exit_status = None
        if not self._open_streams:
            self._channel.shutdown_read()  # Other side could be open by forked child.
        outcome = _SSHCommandOutcome(exit_status) if exit_status is not None else None
        return outcome, stdout, stderr


class _PseudoTerminalSSHCommand(_SSHCommand):
    def __enter__(self):
        self._channel = self._ssh_client.invoke_shell()  # type: paramiko.Channel
        self._logger.debug("Getting banner.")
        stdout = self._channel.recv(100500)  # Receive banner and prompt.
        self._logger.debug("Banner:\n%s", stdout.decode())
        self._logger.debug("Run on %r:\n%s", self, self._script)
        self._channel.send(self._script)
        self._channel.send('\n')
        self._open_streams = {
            "STDOUT": (self._channel.recv, self._logger.getChild('stdout')),
            "STDERR": (self._channel.recv_stderr, self._logger.getChild('stderr')),
            }
        return self

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
        bytes_sent = self._channel.send('\x03')
        self._channel.send('exit\n')
        if bytes_sent == 0:
            raise RuntimeError("Channel already closed")
        assert bytes_sent == 1


class _StraightforwardSSHCommand(_SSHCommand):
    def __enter__(self):
        transport = self._ssh_client.get_transport()
        self._channel = transport.open_session()  # type: paramiko.Channel
        self._logger.debug("Run on %r:\n%s", self, self._script)
        self._channel.exec_command(self._script)
        self._open_streams = {
            "STDOUT": (self._channel.recv, self._logger.getChild('stdout')),
            "STDERR": (self._channel.recv_stderr, self._logger.getChild('stderr')),
            }
        return self

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


class SSH(PosixShell):
    def __init__(self, hostname, port, username, key_path):
        self._hostname = hostname
        self._port = port
        self._username = username
        self._key_path = key_path

    def __repr__(self):
        return 'SSH({!r}, {!r}, {!r}, {!r})'.format(self._username, self._hostname, self._port, self._key_path)

    def command(self, args, cwd=None, env=None):
        script = sh_command_to_script(args)
        return self.sh_script(script, cwd=cwd, env=env)

    def terminal_command(self, args, cwd=None, env=None):
        script = sh_augment_script(sh_command_to_script(args), cwd=cwd, env=env, set_eux=False, shebang=False)
        return _PseudoTerminalSSHCommand(self._client(), script)

    @cached_getter
    def _client(self):
        client = paramiko.SSHClient()
        client.set_missing_host_key_policy(paramiko.MissingHostKeyPolicy())  # Ignore completely.
        try:
            client.connect(
                str(self._hostname), port=self._port,
                username=self._username, key_filename=str(self._key_path),
                look_for_keys=False, allow_agent=False)
        except paramiko.ssh_exception.NoValidConnectionsError as e:
            raise SSHNotConnected("Cannot connect to {}: {} (is port opened?)".format(self, e))
        except paramiko.ssh_exception.SSHException as e:
            raise SSHNotConnected("Cannot connect to {}: {} (is service started? using VirtualBox?)".format(self, e))
        return client

    def __del__(self):
        self._client().close()

    def sh_script(self, script, cwd=None, env=None):
        augmented_script = sh_augment_script(script, cwd, env)
        return _StraightforwardSSHCommand(self._client(), augmented_script)

    def is_working(self):
        try:
            self.run_sh_script(':')
        except SSHNotConnected:
            return False
        return True
