from abc import ABCMeta, abstractmethod

from framework.os_access.exceptions import Timeout, exit_status_error_cls
from framework.waiting import Wait

_DEFAULT_COMMUNICATION_TIMEOUT_SEC = 10


class Command(object):
    __metaclass__ = ABCMeta

    @abstractmethod
    def send(self, bytes_buffer, is_last=False):  # type: (bytes, bool) -> int
        return 0

    @abstractmethod
    def receive(self, timeout_sec):  # type: (float) -> (bytes, bytes)
        return -1, b'', b''

    @abstractmethod
    def terminate(self):
        pass

    def communicate(self, input=None, timeout_sec=None):
        if input is not None:
            # If input bytes not None but empty, send zero bytes once.
            left_to_send = memoryview(input)
            while True:
                left_to_send = left_to_send[self.send(left_to_send, is_last=True):]
                if not left_to_send:
                    break
        stdout = []
        stderr = []
        wait = Wait("data received on stdout and stderr", timeout_sec=timeout_sec, attempts_limit=10000)
        while True:
            exit_code, stdout_chunk, stderr_chunk = self.receive(timeout_sec=wait.delay_sec)
            if stdout_chunk is not None:
                stdout += stdout_chunk
            if stderr_chunk is not None:
                stderr += stderr_chunk
            if exit_code is not None:
                return exit_code, b''.join(stdout), b''.join(stderr)
            if not wait.again():
                raise Timeout(timeout_sec)

    def check_output(self, input=None, timeout_sec=None):
        """Shortcut."""
        with self:
            exit_status, stdout, stderr = self.communicate(input=input, timeout_sec=timeout_sec)
        if exit_status != 0:
            raise exit_status_error_cls(exit_status)(stdout, stderr)
        return stdout
