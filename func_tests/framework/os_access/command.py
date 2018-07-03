import logging
from abc import ABCMeta, abstractmethod, abstractproperty
from contextlib import contextmanager

from framework.os_access.exceptions import Timeout, exit_status_error_cls
from framework.waiting import Wait

_DEFAULT_COMMUNICATION_TIMEOUT_SEC = 10

_logger = logging.getLogger(__name__)


class CommandOutcome(object):
    __metaclass__ = ABCMeta

    def __repr__(self):
        return '<{} {}>'.format(self.__class__.__name__, self.comment)

    @property
    def is_success(self):
        return self.code == 0

    @abstractproperty
    def is_intended_termination(self):
        return False

    @property
    def is_error(self):
        return not self.is_success and not self.is_intended_termination

    @abstractproperty
    def code(self):
        return 0

    @abstractproperty
    def comment(self):
        return u''


class Run(object):
    __metaclass__ = ABCMeta

    @abstractmethod
    def send(self, bytes_buffer, is_last=False):  # type: (bytes, bool) -> int
        return 0

    @abstractmethod
    def receive(self, timeout_sec):
        """Receive stdout chunk and stderr chunk; None if closed"""
        return b'', b''

    @abstractmethod
    def terminate(self):
        pass

    @abstractproperty
    def outcome(self):
        return CommandOutcome()

    def communicate(self, input=None, timeout_sec=_DEFAULT_COMMUNICATION_TIMEOUT_SEC):
        if input is not None:
            # If input bytes not None but empty, send zero bytes once.
            left_to_send = memoryview(input)
            while True:
                left_to_send = left_to_send[self.send(left_to_send, is_last=True):]
                if not left_to_send:
                    break
        streams = {
            'stdout': {'chunks': [], 'closed': False},
            'stderr': {'chunks': [], 'closed': False},
            }
        wait = Wait("data received on stdout and stderr", timeout_sec=timeout_sec, attempts_limit=10000)
        while True:
            stdout_chunk, stderr_chunk = self.receive(timeout_sec=wait.delay_sec)
            streams_open = []
            for name, chunk in [('stdout', stdout_chunk), ('stderr', stderr_chunk)]:
                stream = streams[name]
                if chunk is None:
                    stream['closed'] = True
                else:
                    assert not stream['closed']
                    stream['chunks'].append(chunk)
                    streams_open.append(name)
            if self.outcome is not None and not streams_open:
                _logger.debug("Process exited, streams closed.")
                break
            if not wait.again():
                if self.outcome is not None:
                    assert streams_open
                    _logger.error("Process exited, streams open: %r", streams_open)
                    break
                raise Timeout(timeout_sec)
        for name in ['stdout', 'stderr']:
            yield b''.join(streams[name]['chunks'])


class Command(object):
    __metaclass__ = ABCMeta

    @contextmanager
    def running(self):
        yield Run()

    def check_output(self, input=None, timeout_sec=_DEFAULT_COMMUNICATION_TIMEOUT_SEC):
        """Shortcut."""
        with self.running() as run:
            stdout, stderr = run.communicate(input=input, timeout_sec=timeout_sec)
        if run.outcome.is_error:
            raise exit_status_error_cls(run.outcome.code)(stdout, stderr)
        return stdout
