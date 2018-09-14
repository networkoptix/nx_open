import errno
import logging
import math
import os
import select
import subprocess
import time
from collections import namedtuple
from contextlib import closing

from framework.os_access import posix_shell
from framework.os_access.command import Command, Run

_logger = logging.getLogger(__name__)


class _LocalCommandOutcome(posix_shell.Outcome):
    def __init__(self, pythons_popen_returncode):
        assert isinstance(pythons_popen_returncode, int)
        assert -31 <= pythons_popen_returncode <= 255  # See: https://bugs.python.org/issue27167
        self._returncode = pythons_popen_returncode

    @property
    def signal(self):
        # See: subprocess.Popen#_handle_exitstatus
        # See: https://bugs.python.org/issue27167
        if self._returncode >= 0:
            return None
        return -self._returncode

    @property
    def code(self):
        if self.signal:
            return 128 + self.signal
        return self._returncode


class _LocalRun(Run):
    _Stream = namedtuple('_Stream', ['name', 'file_obj', 'logger'])

    def __init__(self, logger, *popenargs, **popenkwargs):
        self._logger = logger
        self._process = subprocess.Popen(*popenargs, **popenkwargs)
        self._poller = select.poll()
        self._streams = {}  # Indexed by their fd.
        if self._process.stdout:
            self._register_and_append('stdout', self._process.stdout, select.POLLIN | select.POLLPRI)
        if self._process.stderr:
            self._register_and_append('stderr', self._process.stderr, select.POLLIN | select.POLLPRI)

    def _register_and_append(self, name, file_obj, eventmask):
        fd = file_obj.fileno()
        self._streams[fd] = self._Stream(name, file_obj, self._logger.getChild(name))
        self._streams[fd].logger.debug("Register file descriptor %d for %s, event mask %x.", fd, name, eventmask)
        self._poller.register(fd, eventmask)

    def _close_unregister_and_remove(self, fd):
        self._streams[fd].logger.debug("Unregister file descriptor %d.", fd)
        self._poller.unregister(fd)
        self._streams[fd].file_obj.close()
        del self._streams[fd]

    @property
    def logger(self):
        return self._logger

    def send(self, bytes_buffer, is_last=False):
        # Blocking call, no need to use polling functionality.
        try:
            bytes_written = os.write(self._process.stdin.fileno(), bytes_buffer[:posix_shell.STREAM_BUFFER_SIZE])
            if bytes_buffer:
                assert 0 < bytes_written <= len(bytes_buffer)
            else:
                assert bytes_written == 0
            if is_last and bytes_written == len(bytes_buffer):
                self._process.stdin.close()
            return bytes_written
        except OSError as e:
            if e.errno != errno.EPIPE:
                raise
            # Ignore EPIPE: -- behave as if everything has been written.
            self._logger.getChild('stdin').debug("EPIPE.")
            self._process.stdin.close()
            return len(bytes_buffer)

    @property
    def outcome(self):
        exit_status = self._process.poll()
        if exit_status is not None:
            return _LocalCommandOutcome(exit_status)
        return None

    def _poll_exit_status(self, timeout_sec):
        # TODO: Use combination of alert() and os.waitpid(): https://stackoverflow.com/a/282190/1833960.
        if self._process.poll() is None:
            self._logger.debug("Hasn't exited. Sleep for %.3f seconds.", timeout_sec)
            time.sleep(timeout_sec)
        else:
            self._logger.info("Process exited, not streams open -- this must be last call.")
        return self._process.poll()

    def _poll_streams(self, timeout_sec):
        try:
            return self._poller.poll(int(math.ceil(timeout_sec * 1000)))
        except select.error as e:
            if e.args[0] != errno.EINTR:
                raise
            self._logger.debug("Got EINTR. No problem, go on.")
            return []

    def receive(self, timeout_sec):
        name2data = {}
        if self._streams:
            for fd, mode in self._poll_streams(timeout_sec):
                stream = self._streams[fd]
                if mode & (select.POLLIN | select.POLLPRI):
                    if mode & select.POLLHUP:
                        stream.logger.debug("Ended; ready.")
                    else:
                        stream.logger.debug("Ready.")
                    chunk = os.read(fd, posix_shell.STREAM_BUFFER_SIZE)
                    assert chunk, "Must be some data: see local variable `mode`"
                    name2data[stream.name] = chunk
                elif mode & select.POLLHUP:
                    stream.logger.debug("Ended.")
                    self._close_unregister_and_remove(fd)
                else:
                    raise RuntimeError("Poll mode: %d", mode)
        for stream in self._streams.values():
            name2data.setdefault(stream.name, b'')
        return name2data.get('stdout'), name2data.get('stderr')

    def terminate(self):
        self._process.send_signal(2)  # SIGINT, which is sent by terminal when pressing Ctrl+C.

    def close(self):
        for fd in list(self._streams):  # List is preserved during iteration. Dict is emptying.
            self._close_unregister_and_remove(fd)
        self._logger.debug("All file descriptors are closed.")


class _LocalCommand(Command):
    """See `_communicate` from Python's standard library."""

    def __init__(self, logger, *popenargs, **popenkwargs):
        self._logger = logger
        self.popenargs = popenargs
        self.popenkwargs = popenkwargs

    def running(self):
        return closing(_LocalRun(self._logger, *self.popenargs, **self.popenkwargs))


class _LocalShell(posix_shell.Shell):
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

    def is_working(self):
        return True

    @classmethod
    def command(cls, command, cwd=None, env=None, logger=None, set_eux=False):
        if not logger:
            logger = _logger
        if set_eux:
            return cls.sh_script(posix_shell.command_to_script(command), cwd=cwd, env=env, set_eux=set_eux)
        kwargs = cls._make_kwargs(cwd, env)
        command = [str(arg) for arg in command]
        logger.info('Run local command:\n%s', posix_shell.command_to_script(command))
        return _LocalCommand(logger, command, shell=False, **kwargs)

    @classmethod
    def sh_script(cls, script, cwd=None, env=None, logger=None, set_eux=True):
        if not logger:
            logger = _logger
        augmented_script_to_run = posix_shell.augment_script(script, set_eux=set_eux)
        augmented_script_to_log = posix_shell.augment_script(script, cwd=cwd, env=env, set_eux=set_eux)
        logger.info('Run local script:\n%s', augmented_script_to_log)
        kwargs = cls._make_kwargs(cwd, posix_shell.env_values_to_str(env) if env else None)
        return _LocalCommand(logger, augmented_script_to_run, shell=True, **kwargs)


local_shell = _LocalShell()
