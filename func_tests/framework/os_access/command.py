import logging
import timeit
from abc import ABCMeta, abstractmethod, abstractproperty
from contextlib import contextmanager

from framework.os_access.exceptions import Timeout, exit_status_error_cls
from framework.waiting import Wait

_logger = logging.getLogger(__name__)

DEFAULT_RUN_TIMEOUT_SEC = 60

# vboxmanage output can be is 4096 bytes / 154 lines, and we may want to see it
_BIG_CHUNK_THRESHOLD_BYTES = 10000
_BIG_CHUNK_THRESHOLD_LINES = 200


class _Buffer(object):
    _line_beginning_skipped = u"(Line beginning was skipped.) "

    def __init__(self, name, logger):
        self._name = name
        self._chunks = []
        self._considered_binary = False
        self._logger = logger.getChild(name)
        self._next_indent = ''
        self.closed = False

    def __repr__(self):
        return '<{!s}: {!s}>'.format(self._name, 'closed' if self.closed else 'open')

    def _get_log_entry(self, chunk):
        """Small text chunks are logged, unfinished lines continue where ended"""
        if self._considered_binary or len(chunk) > _BIG_CHUNK_THRESHOLD_BYTES:
            return "%d bytes.", len(chunk)  # Considered as binary.
        if len(chunk) > _BIG_CHUNK_THRESHOLD_BYTES:
            self._next_indent = self._line_beginning_skipped
            return "%d bytes.", len(chunk)  # Too big, won't decode.
        try:
            decoded = chunk.decode()
        except UnicodeDecodeError:
            self._considered_binary = True
            return "%d bytes.", len(chunk)  # Can't decode, consider as binary.
        lines_number = decoded.count(u"\n") + 1
        if lines_number > _BIG_CHUNK_THRESHOLD_LINES:
            self._next_indent = self._line_beginning_skipped
            return "%d lines, %d characters.", lines_number, len(decoded)  # Too many lines.
        this_indent = self._next_indent
        if decoded.endswith('\n'):
            self._next_indent = ''
            return u"\n%s%s", this_indent, decoded[:-1]
        if lines_number == 1:
            if self._next_indent != self._line_beginning_skipped:
                self._next_indent += ' ' * len(decoded)
        else:
            last_new_line = decoded.rfind(u'\n')
            self._next_indent = ' ' * (len(decoded) - last_new_line - 1)
        return u"\n%s%s", this_indent, decoded

    def write(self, chunk):
        if chunk is None:
            if not self.closed:
                self.closed = True
                self._logger.debug("Closed.")
        else:
            assert not self.closed
            if chunk:
                self._chunks.append(chunk)
                self._logger.info(*self._get_log_entry(chunk))

    def read(self):
        data = b''.join(self._chunks)
        self._chunks = []
        return data


class _Buffers(object):
    def __init__(self, logger):
        self.stdout = _Buffer('stdout', logger)
        self.stderr = _Buffer('stderr', logger)

    def __repr__(self):
        return '<{!r}, {!r}>'.format(self.stdout, self.stderr)

    def __iter__(self):
        yield self.stdout
        yield self.stderr

    @property
    def closed(self):
        return all(buffer.closed for buffer in self)


class CommandOutcome(object):
    __metaclass__ = ABCMeta

    def __str__(self):
        return self.comment

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

    @abstractproperty
    def logger(self):
        return _logger

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

    def expect(self, expected_bytes, timeout_sec=10):
        """Wait for and compare specific bytes in stdout; named after `pexpect` lib."""
        pos = 0
        time_left_sec = timeout_sec
        stderr_buffer = _Buffer('stderr', self.logger)
        while pos < len(expected_bytes):
            started_at = timeit.default_timer()
            stdout, stderr = self.receive(timeout_sec=time_left_sec)
            stderr_buffer.write(stderr)
            time_left_sec -= timeit.default_timer() - started_at
            if stdout is None:
                self.communicate(timeout_sec=time_left_sec)
                assert self.outcome is not None
                assert self.outcome.is_error
                raise exit_status_error_cls(self.outcome.code)(None, stderr_buffer.read())
            if not expected_bytes.startswith(stdout, pos):
                raise RuntimeError("Expected %r; left: %r; got: %r.", expected_bytes, expected_bytes[pos:], stdout)
            pos += len(stdout)

    def communicate(self, input=None, timeout_sec=DEFAULT_RUN_TIMEOUT_SEC):
        if input is not None:
            # If input bytes not None but empty, send zero bytes once.
            left_to_send = memoryview(input)
            while True:
                left_to_send = left_to_send[self.send(left_to_send, is_last=True):]
                if not left_to_send:
                    break
        buffers = _Buffers(self.logger)
        wait = Wait(
            "data received on stdout and stderr",
            timeout_sec=timeout_sec,
            attempts_limit=10000,
            logger=self.logger.getChild('wait'),
            )
        while True:
            self.logger.debug("Receive data, timeout: %f sec", wait.delay_sec)
            chunks = self.receive(timeout_sec=wait.delay_sec)
            for buffer, chunk in zip(buffers, chunks):
                buffer.write(chunk)
            if self.outcome and not self.outcome.is_success:
                outcome_log_fn = self.logger.info
            else:
                outcome_log_fn = self.logger.debug
            outcome_log_fn("Outcome: %s; buffers: %r.", self.outcome, buffers)
            if self.outcome is not None and buffers.closed:
                self.logger.debug("Exit clean.")
                break
            if not wait.again():
                if self.outcome is not None:
                    self.logger.debug("Exit with streams not closed.")
                    break
                raise Timeout(timeout_sec)
        return buffers.stdout.read(), buffers.stderr.read()


class Command(object):
    __metaclass__ = ABCMeta

    @contextmanager
    def running(self):
        yield Run()

    def check_output(self, input=None, timeout_sec=DEFAULT_RUN_TIMEOUT_SEC):
        """Shortcut."""
        with self.running() as run:
            stdout, stderr = run.communicate(input=input, timeout_sec=timeout_sec)
        if run.outcome.is_error:
            raise exit_status_error_cls(run.outcome.code)(stdout, stderr)
        return stdout
