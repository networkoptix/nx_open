import errno
import logging
import math
import os
import select
import subprocess
import time

from framework.os_access.command import Command
from framework.os_access.posix_shell import PosixShell, _DEFAULT_TIMEOUT_SEC, _STREAM_BUFFER_SIZE
from framework.os_access.posix_shell_utils import sh_augment_script, sh_command_to_script

_logger = logging.getLogger(__name__)


class _LocalCommand(Command):
    """See `_communicate` from Python's standard library."""

    def __init__(self, *popenargs, **popenkwargs):
        self.popenargs = popenargs
        self.popenkwargs = popenkwargs

    def __enter__(self):
        self.process = subprocess.Popen(*self.popenargs, **self.popenkwargs)
        self.poller = select.poll()
        process_logger = _logger.getChild('pid{}'.format(self.process.pid))
        self.interaction_logger = process_logger.getChild('interaction')
        self.interaction_logger.setLevel(logging.INFO)  # Too verbose for everyday usage.
        self.fd2name = {}
        self.fd2file = {}
        if self.process.stdout:
            self.register_and_append('stdout', self.process.stdout, select.POLLIN | select.POLLPRI)
        if self.process.stderr:
            self.register_and_append('stderr', self.process.stderr, select.POLLIN | select.POLLPRI)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        for fd in list(self.fd2file):  # List is preserved during iteration. Dict is emptying.
            self.close_unregister_and_remove(fd)
        self.interaction_logger.debug("All file descriptors are closed.")

    def register_and_append(self, name, file_obj, eventmask):
        fd = file_obj.fileno()
        self.interaction_logger.debug("Register file descriptor %d, event mask %x.", fd, eventmask)
        self.poller.register(fd, eventmask)
        self.fd2file[fd] = file_obj
        self.fd2name[fd] = name

    def close_unregister_and_remove(self, fd):
        self.interaction_logger.debug("Unregister file descriptor %d.", fd)
        self.poller.unregister(fd)
        self.fd2file[fd].close()
        self.fd2file.pop(fd)

    def send(self, bytes_buffer, is_last=False):
        # Blocking call, no need to use polling functionality.
        try:
            bytes_written = os.write(self.process.stdin.fileno(), bytes_buffer)
            assert 0 < bytes_written <= len(bytes_buffer)
            if is_last and bytes_written == len(bytes_buffer):
                self.process.stdin.close()
            return bytes_written
        except OSError as e:
            if e.errno != errno.EPIPE:
                raise
            # Ignore EPIPE: -- behave as if everything has been written.
            self.interaction_logger.debug("EPIPE on STDIN.")
            self.process.stdin.close()
            return len(bytes_buffer)

    def _receive_exit_status(self, timeout_sec):
        # TODO: When moved to Python 3, simply use `self.process.wait(timeout_sec)`.
        assert not self.fd2file
        if self.process.poll() is None:
            self.interaction_logger.debug("Hasn't exited. Sleep for %.3f seconds.", timeout_sec)
            time.sleep(timeout_sec)
        return self.process.poll(), None, None

    def receive(self, timeout_sec):
        if not self.fd2file:
            return self._receive_exit_status(timeout_sec)

        try:
            ready = self.poller.poll(int(math.ceil(timeout_sec * 1000)))
        except select.error as e:
            if e.args[0] == errno.EINTR:
                self.interaction_logger.debug("Got EINTR.")
                return None, None, None
            raise

        if not ready:
            self.interaction_logger.warning("Nothing on streams. Some still open.")
            if self.process.poll() is not None:
                self.interaction_logger.error("Process exited but some streams open. Ignore them.")
                return self.process.poll(), None, None
            return None, None, None

        name2data = {}
        for fd, mode in ready:
            if mode & (select.POLLIN | select.POLLPRI):
                if not self.process.stdout.closed and fd == self.process.stdout.fileno():
                    self.interaction_logger.debug("STDOUT ready.")
                elif not self.process.stderr.closed and fd == self.process.stderr.fileno():
                    self.interaction_logger.debug("STDERR ready.")
                else:
                    assert False
                data = os.read(fd, _STREAM_BUFFER_SIZE)
                if not data:
                    self.interaction_logger.debug("No data (%r), close %d.", fd)
                    self.close_unregister_and_remove(fd)
                name2data[self.fd2name[fd]] = data
            else:
                # Ignore hang up or errors.
                self.close_unregister_and_remove(fd)

        return self.process.poll(), name2data.get('stdout'), name2data.get('stderr')


class _LocalShell(PosixShell):
    def __repr__(self):
        return '<LocalShell>'

    @staticmethod
    def _make_kwargs(cwd, env):
        kwargs = {
            'close_fds': True,
            'bufsize': 16 * 1024 * 1024,
            'stdout': subprocess.PIPE,
            'stderr': subprocess.PIPE,
            'stdin': subprocess.PIPE}
        if cwd is not None:
            kwargs['cwd'] = str(cwd)
        if env is not None:
            kwargs['env'] = {name: str(value) for name, value in env.items()}
        return kwargs

    @classmethod
    def command(cls, command, cwd=None, env=None, timeout_sec=_DEFAULT_TIMEOUT_SEC):
        kwargs = cls._make_kwargs(cwd, env)
        command = [str(arg) for arg in command]
        _logger.debug('Run command:\n%s', sh_command_to_script(command))
        return _LocalCommand(command, shell=False, **kwargs)

    @classmethod
    def sh_script(cls, script, cwd=None, env=None, timeout_sec=_DEFAULT_TIMEOUT_SEC):
        augmented_script_to_run = sh_augment_script(script, None, None)
        augmented_script_to_log = sh_augment_script(script, cwd, env)
        _logger.debug('Run:\n%s', augmented_script_to_log)
        kwargs = cls._make_kwargs(cwd, env)
        return _LocalCommand(augmented_script_to_run, shell=True, **kwargs)


local_shell = _LocalShell()
