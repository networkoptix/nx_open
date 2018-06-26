import errno
import logging
import math
import os
import select
import subprocess

from framework.os_access.exceptions import Timeout, exit_status_error_cls
from framework.os_access.posix_shell import PosixShell, _DEFAULT_TIMEOUT_SEC, _LoggedOutputBuffer, _STREAM_BUFFER_SIZE
from framework.os_access.posix_shell_utils import sh_augment_script, sh_command_to_script
from framework.waiting import Wait

_logger = logging.getLogger(__name__)


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
                chunk = input[input_offset: input_offset + _STREAM_BUFFER_SIZE]
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
                data = os.read(fd, _STREAM_BUFFER_SIZE)
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
    def run_command(cls, command, input=None, cwd=None, env=None, timeout_sec=_DEFAULT_TIMEOUT_SEC):
        kwargs = cls._make_kwargs(cwd, env, input is not None)
        command = [str(arg) for arg in command]
        _logger.debug('Run command:\n%s', sh_command_to_script(command))
        process = subprocess.Popen(command, shell=False, **kwargs)
        return cls._communicate(process, input, timeout_sec)

    @classmethod
    def run_sh_script(cls, script, input=None, cwd=None, env=None, timeout_sec=_DEFAULT_TIMEOUT_SEC):
        augmented_script_to_run = sh_augment_script(script, None, None)
        augmented_script_to_log = sh_augment_script(script, cwd, env)
        _logger.debug('Run:\n%s', augmented_script_to_log)
        kwargs = cls._make_kwargs(cwd, env, input is not None)
        process = subprocess.Popen(augmented_script_to_run, shell=True, **kwargs)
        return cls._communicate(process, input, timeout_sec)


local_shell = _LocalShell()
