import errno
import logging
import math
import os
import select
import socket
import subprocess
from abc import ABCMeta, abstractmethod

import paramiko
from pylru import lrudecorator

from framework.os_access.exceptions import Timeout, exit_status_error_cls
from framework.os_access.posix_shell_utils import sh_augment_script, sh_command_to_script
from framework.waiting import Wait

_logger = logging.getLogger(__name__)


class PosixShell(object):
    """Posix-specific interface"""
    __metaclass__ = ABCMeta

    @abstractmethod
    def run_command(self, command, input=None, cwd=None, timeout_sec=60, env=None):
        return b'stdout'

    @abstractmethod
    def run_sh_script(self, script, input=None, cwd=None, timeout_sec=60, env=None):
        return b'stdout'


class SSHNotConnected(Exception):
    pass


class SSH(PosixShell):
    def __init__(self, hostname, port, username, key_path):
        self._hostname = hostname
        self._port = port
        self._username = username
        self._key_path = key_path

    def __repr__(self):
        return '<SSH: ssh {}@{} -p {:d} -i {}>'.format(self._username, self._hostname, self._port, self._key_path)

    def run_command(self, command, input=None, cwd=None, timeout_sec=60, env=None):
        script = sh_command_to_script(command)
        output = self.run_sh_script(script, input=input, cwd=cwd, timeout_sec=timeout_sec, env=env)
        return output

    @lrudecorator(100)
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

    def _channel(self):
        transport = self._client().get_transport()
        channel = transport.open_session()
        return channel

    @staticmethod
    def _send_input(channel, input):
        if input is not None:
            channel.settimeout(10)  # Must never time out assuming process always open stdin and read from it.
            input_offset = 0
            while True:
                assert 0 <= input_offset <= len(input)
                if input_offset == len(input):  # If input is empty, nothing is sent.
                    _logger.debug("Input sent successfully.")
                    break
                input_chunk = input[input_offset:input_offset + 64 * 1024]
                input_bytes_sent = channel.send(input_chunk)
                input_offset += input_bytes_sent
                if input_bytes_sent == 0:
                    _logger.warning("Write direction preliminary closed from the other side.")
                    break
                _logger.debug("Chunk of input sent: %d bytes.", input_bytes_sent)
        _logger.debug("Shutdown write direction.")
        channel.shutdown_write()

    def run_sh_script(self, script, input=None, cwd=None, timeout_sec=60, env=None):
        augmented_script = sh_augment_script(script, cwd, env)
        subprocess_logger = _logger.getChild('subprocess')
        subprocess_logger.debug("Run on %r:\n%s", self, augmented_script)
        channel = self._channel()
        channel.exec_command(augmented_script)

        self._send_input(channel, input)

        wait_for_data = Wait("data received on stdout and stderr", timeout_sec=timeout_sec, attempts_limit=10000)
        stdout_chunks = []
        stderr_chunks = []
        open_streams = {
            "STDOUT": (channel.recv, stdout_chunks, subprocess_logger.getChild('stdout')),
            "STDERR": (channel.recv_stderr, stderr_chunks, subprocess_logger.getChild('stderr')),
            }
        while open_streams:
            channel.settimeout(wait_for_data.delay_sec)
            subprocess_responded = False
            for key, (recv, output_chunks, output_logger) in open_streams.items():
                try:
                    output_chunk = recv(1024 * 1024)
                except socket.timeout:
                    continue  # Next stream might have data on it.
                subprocess_responded = True
                if not output_chunk:
                    output_logger.debug("Closed from the other side.")
                    del open_streams[key]
                    break  # Avoid iteration over mutated dict.
                output_logger.debug("Received: %d bytes.", len(output_chunk))
                output_chunks.append(output_chunk)
                if len(output_chunk) > 10000:
                    output_logger.debug("Big chunk: %d bytes", len(output_chunk))
                else:
                    try:
                        output_logger.debug("Text:\n%s", output_chunk.decode('ascii'))
                    except UnicodeDecodeError:
                        output_logger.debug("Binary: %d bytes", len(output_chunk))
            if not subprocess_responded:
                subprocess_logger.debug("Exit status: %r.", channel.exit_status)
                if channel.exit_status != -1:
                    assert 0 <= channel.exit_status <= 255
                    subprocess_logger.error("Remote process exited but streams left open (child forked?)")
                    break  # Leave
                if not wait_for_data.again():
                    raise Timeout(timeout_sec)

        subprocess_logger.debug("No data will be read from stdout and stderr.")
        stdout = b''.join(stdout_chunks)
        stderr = b''.join(stderr_chunks)

        while True:
            if channel.exit_status_ready():
                subprocess_logger.debug("Process exit status: %d.", channel.exit_status)
                if channel.exit_status != 0:
                    raise exit_status_error_cls(channel.exit_status)(stdout, stderr)
                break
            subprocess_logger.debug("Process still not exited")

        channel.shutdown_read()  # Other side could be open by forked child.

        return stdout

    def is_working(self):
        try:
            self.run_sh_script(':')
        except SSHNotConnected:
            return False
        return True


class _LoggedOutputBuffer(object):
    def __init__(self, logger):
        self.chunks = []
        self._considered_binary = False
        self._logger = logger
        self._indent = 0

    def append(self, chunk):
        self.chunks.append(chunk)
        if not self._considered_binary:
            try:
                decoded = chunk.decode()
            except UnicodeDecodeError:
                self._considered_binary = True
            else:
                # Potentially expensive.
                if len(decoded) < 5000 and decoded.count('\n') < 50:
                    self._logger.debug(u'\n%s', decoded)
                else:
                    self._logger.debug('%d characters.', len(decoded))
        if self._considered_binary:  # Property may be changed, and, therefore, both if's may be entered.
            self._logger.debug('%d bytes.', len(chunk))


def _communicate(process, input, timeout_sec):
    """Patched version of method with same name from standard Popen class"""
    process_logger = _logger.getChild('pid{}'.format(process.pid))
    interaction_logger = process_logger.getChild('interaction')
    interaction_logger.setLevel(logging.INFO)  # Too verbose for everyday usage.

    stdout = None  # Return
    stderr = None  # Return
    fd2file = {}
    fd2output = {}

    poller = select.poll()

    def register_and_append(file_obj, eventmask):
        interaction_logger.debug("Register file descriptor %d, event mask %x.", file_obj.fileno(), eventmask)
        poller.register(file_obj.fileno(), eventmask)
        fd2file[file_obj.fileno()] = file_obj

    def close_unregister_and_remove(fd):
        interaction_logger.debug("Unregister file descriptor %d.", fd)
        poller.unregister(fd)
        fd2file[fd].close()
        fd2file.pop(fd)

    if process.stdin and input is not None:
        register_and_append(process.stdin, select.POLLOUT)

    if process.stdout:
        register_and_append(process.stdout, select.POLLIN | select.POLLPRI)
        fd2output[process.stdout.fileno()] = stdout = _LoggedOutputBuffer(process_logger.getChild('stdout'))
    if process.stderr:
        register_and_append(process.stderr, select.POLLIN | select.POLLPRI)
        fd2output[process.stderr.fileno()] = stderr = _LoggedOutputBuffer(process_logger.getChild('stderr'))

    input_offset = 0

    wait = Wait(
        'process responds',
        timeout_sec=timeout_sec,
        log_continue=process_logger.getChild('wait').debug,
        log_stop=process_logger.getChild('wait').error)

    while fd2file:
        try:
            ready = poller.poll(int(math.ceil(wait.delay_sec * 1000)))
        except select.error as e:
            if e.args[0] == errno.EINTR:
                interaction_logger.debug("Got EINTR.")
                continue
            raise

        if not ready:
            interaction_logger.warning("Nothing on streams. Some still open.")
            if process.poll() is not None:
                interaction_logger.error("Process exited but some streams open. Ignore them.")
                break
            if not wait.again():
                break
            continue

        for fd, mode in ready:
            if mode & select.POLLOUT:
                interaction_logger.debug("STDIN ready.")
                assert fd == process.stdin.fileno()
                chunk = input[input_offset: input_offset + 16 * 1024]
                try:
                    interaction_logger.debug("Write %d bytes.", len(chunk))
                    input_offset += os.write(fd, chunk)
                except OSError as e:
                    if e.errno == errno.EPIPE:
                        interaction_logger.debug("EPIPE on STDIN.")
                        close_unregister_and_remove(fd)
                    else:
                        raise
                else:
                    if input_offset >= len(input):
                        interaction_logger.debug("Everything is transmitted to STDIN.")
                        close_unregister_and_remove(fd)
            elif mode & (select.POLLIN | select.POLLPRI):
                if not process.stdout.closed and fd == process.stdout.fileno():
                    interaction_logger.debug("STDOUT ready.")
                elif not process.stderr.closed and fd == process.stderr.fileno():
                    interaction_logger.debug("STDERR ready.")
                else:
                    assert False
                data = os.read(fd, 16 * 1024)
                if not data:
                    interaction_logger.debug("No data (%r), close %d.", fd)
                    close_unregister_and_remove(fd)
                fd2output[fd].append(data)
            else:
                # Ignore hang up or errors.
                close_unregister_and_remove(fd)

    for fd in list(fd2file):  # List is preserved during iteration. Dict is emptying.
        close_unregister_and_remove(fd)
    interaction_logger.debug("All file descriptors are closed.")

    while True:
        exit_status = process.poll()
        if exit_status is not None:
            interaction_logger.debug("Polled and got exit status: %d.", exit_status)
            break
        interaction_logger.debug("Polled and got no exit status.")
        if not wait.again():
            interaction_logger.debug("No more waiting for exit.")
            break
        interaction_logger.debug("Wait for exit.")
        wait.sleep()

    return exit_status, b''.join(stdout.chunks), b''.join(stderr.chunks)  # TODO: Return io.BytesIO.


class _LocalShell(PosixShell):
    def __repr__(self):
        return '<LocalShell>'

    @staticmethod
    def _make_kwargs(cwd, env, has_input):
        kwargs = {
            'close_fds': True,
            'bufsize': 16 * 1024 * 1024,
            'stdout': subprocess.PIPE,
            'stderr': subprocess.PIPE,
            'stdin': subprocess.PIPE if has_input else None}
        if cwd is not None:
            kwargs['cwd'] = str(cwd)
        if env is not None:
            kwargs['env'] = {name: str(value) for name, value in env.items()}
        return kwargs

    @classmethod
    def _communicate(cls, process, input, timeout_sec):
        exit_status, stdout, stderr = _communicate(process, input, timeout_sec)
        if exit_status is None:
            raise Timeout(timeout_sec)
        if exit_status != 0:
            raise exit_status_error_cls(exit_status)(stdout, stderr)
        return stdout

    @classmethod
    def run_command(cls, command, input=None, cwd=None, env=None, timeout_sec=60 * 60):
        kwargs = cls._make_kwargs(cwd, env, input is not None)
        command = [str(arg) for arg in command]
        _logger.debug('Run command:\n%s', sh_command_to_script(command))
        process = subprocess.Popen(command, shell=False, **kwargs)
        return cls._communicate(process, input, timeout_sec)

    @classmethod
    def run_sh_script(cls, script, input=None, cwd=None, env=None, timeout_sec=60 * 60):
        augmented_script_to_run = sh_augment_script(script, None, None)
        augmented_script_to_log = sh_augment_script(script, cwd, env)
        _logger.debug('Run:\n%s', augmented_script_to_log)
        kwargs = cls._make_kwargs(cwd, env, input is not None)
        process = subprocess.Popen(augmented_script_to_run, shell=True, **kwargs)
        return cls._communicate(process, input, timeout_sec)


local_shell = _LocalShell()
