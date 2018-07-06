import errno
import logging
import math
import os
import select

from framework.waiting import Wait

logger = logging.getLogger(__name__)


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


def communicate(process, input, timeout_sec):
    """Patched version of method with same name from standard Popen class"""
    process_logger = logger.getChild(str(process.pid))

    stdout = None  # Return
    stderr = None  # Return
    fd2file = {}
    fd2output = {}

    poller = select.poll()

    def register_and_append(file_obj, eventmask):
        poller.register(file_obj.fileno(), eventmask)
        fd2file[file_obj.fileno()] = file_obj

    def close_unregister_and_remove(fd):
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
        log_continue=process_logger.debug,
        log_stop=process_logger.error)

    while fd2file:
        try:
            ready = poller.poll(int(math.ceil(wait.delay_sec * 1000)))
        except select.error as e:
            if e.args[0] == errno.EINTR:
                continue
            raise

        for fd, mode in ready:
            if mode & select.POLLOUT:
                chunk = input[input_offset: input_offset + 16 * 1024]
                try:
                    input_offset += os.write(fd, chunk)
                except OSError as e:
                    if e.errno == errno.EPIPE:
                        close_unregister_and_remove(fd)
                    else:
                        raise
                else:
                    if input_offset >= len(input):
                        close_unregister_and_remove(fd)
            elif mode & (select.POLLIN | select.POLLPRI):
                data = os.read(fd, 16 * 1024)
                if not data:
                    close_unregister_and_remove(fd)
                fd2output[fd].append(data)
            else:
                # Ignore hang up or errors.
                close_unregister_and_remove(fd)
        else:
            if not wait.again():
                for fd in list(fd2file):
                    close_unregister_and_remove(fd)
                break  # Unnecessary but explicit.

    while True:
        exit_status = process.poll()
        if exit_status is not None:
            break
        if not wait.again():
            break
        wait.sleep()

    return exit_status, b''.join(stdout.chunks), b''.join(stderr.chunks)
