import logging
import socket

import paramiko

from framework.method_caching import cached_getter
from framework.os_access.command import Command
from framework.os_access.exceptions import exit_status_error_cls
from framework.os_access.posix_shell import (
    PosixShell,
    _BIG_CHUNK_THRESHOLD_CHARS,
    _DEFAULT_TIMEOUT_SEC,
    _STREAM_BUFFER_SIZE,
    )
from framework.os_access.posix_shell_utils import sh_augment_script, sh_command_to_script

_logger = logging.getLogger(__name__)


class SSHNotConnected(Exception):
    pass


class _SSHCommand(Command):
    _logger = _logger.getChild('_SSHCommand')

    def __init__(self, ssh_client, script):
        super(_SSHCommand, self).__init__()
        self._ssh_client = ssh_client
        self._script = script

    def __enter__(self):
        transport = self._ssh_client.get_transport()
        self._channel = transport.open_session()
        self._logger.debug("Run on %r:\n%s", self, self._script)
        self._channel.exec_command(self._script)
        self._open_streams = {
            "STDOUT": (self._channel.recv, self._logger.getChild('stdout')),
            "STDERR": (self._channel.recv_stderr, self._logger.getChild('stderr')),
            }
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        pass

    def send(self, input, is_last=False):
        self._channel.settimeout(10)  # Must never time out assuming process always open stdin and read from it.
        input = memoryview(input)
        bytes_sent = self._channel.send(input[:_STREAM_BUFFER_SIZE])
        _logger.debug("Chunk of input sent: %d bytes.", bytes_sent)
        if bytes_sent == 0:
            _logger.warning("Write direction preliminary closed from the other side.")
            # TODO: Raise exception.
        if is_last and not input[bytes_sent:]:
            _logger.debug("Shutdown write direction.")
            self._channel.shutdown_write()
        return bytes_sent

    def receive(self, timeout_sec):
        self._channel.settimeout(timeout_sec)
        output_chunks = {}
        for key, (recv, output_logger) in self._open_streams.items():
            try:
                # TODO: Wait on `self.channel.event`.
                output_chunk = recv(_STREAM_BUFFER_SIZE)
            except socket.timeout:
                continue  # Next stream might have data on it.
            if not output_chunk:
                output_logger.debug("Closed from the other side.")
                del self._open_streams[key]
                break  # Avoid iteration over mutated dict.
            output_logger.debug("Received: %d bytes.", len(output_chunk))
            output_chunks[key] = output_chunk
            if len(output_chunk) > _BIG_CHUNK_THRESHOLD_CHARS:
                output_logger.debug("Big chunk: %d bytes", len(output_chunk))
            else:
                try:
                    output_logger.debug("Text:\n%s", output_chunk.decode('ascii'))
                except UnicodeDecodeError:
                    output_logger.debug("Binary: %d bytes", len(output_chunk))
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
        return exit_status, output_chunks.get('STDOUT'), output_chunks.get('STDERR')


class SSH(PosixShell):
    def __init__(self, hostname, port, username, key_path):
        self._hostname = hostname
        self._port = port
        self._username = username
        self._key_path = key_path

    def __repr__(self):
        return 'SSH({!r}, {!r}, {!r}, {!r})'.format(self._username, self._hostname, self._port, self._key_path)

    def run_command(self, command, input=None, cwd=None, timeout_sec=_DEFAULT_TIMEOUT_SEC, env=None):
        script = sh_command_to_script(command)
        output = self.run_sh_script(script, input=input, cwd=cwd, timeout_sec=timeout_sec, env=env)
        return output

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

    def run_sh_script(self, script, input=None, cwd=None, timeout_sec=_DEFAULT_TIMEOUT_SEC, env=None):
        augmented_script = sh_augment_script(script, cwd, env)
        with _SSHCommand(self._client(), augmented_script) as command:
            exit_status, stdout, stderr = command.communicate(input=input, timeout_sec=timeout_sec)
        if exit_status != 0:
            raise exit_status_error_cls(exit_status)(stdout, stderr)
        return stdout

    def is_working(self):
        try:
            self.run_sh_script(':')
        except SSHNotConnected:
            return False
        return True
