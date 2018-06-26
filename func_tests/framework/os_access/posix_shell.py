from abc import ABCMeta, abstractmethod

from framework.os_access.command import Command

_DEFAULT_TIMEOUT_SEC = 60
_BIG_CHUNK_THRESHOLD_CHARS = 10000
_BIG_CHUNK_THRESHOLD_LINES = 50
_STREAM_BUFFER_SIZE = 16 * 1024


class PosixShell(object):
    """Posix-specific interface"""
    __metaclass__ = ABCMeta

    @abstractmethod
    def command(self, args, cwd=None, env=None):
        return Command()

    @abstractmethod
    def sh_script(self, script, cwd=None, env=None):
        return Command()

    def run_command(self, args, input=None, cwd=None, timeout_sec=_DEFAULT_TIMEOUT_SEC, env=None):
        """Shortcut. Deprecated."""
        return self.command(args, cwd=cwd, env=env).check_output(input=input,timeout_sec=timeout_sec)

    def run_sh_script(self, script, input=None, cwd=None, timeout_sec=_DEFAULT_TIMEOUT_SEC, env=None):
        """Shortcut. Deprecated."""
        return self.sh_script(script, cwd=cwd, env=env).check_output(input=input, timeout_sec=timeout_sec)


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
                if len(decoded) < _BIG_CHUNK_THRESHOLD_CHARS and decoded.count('\n') < _BIG_CHUNK_THRESHOLD_LINES:
                    self._logger.debug(u'\n%s', decoded)
                else:
                    self._logger.debug('%d characters.', len(decoded))
        if self._considered_binary:  # Property may be changed, and, therefore, both if's may be entered.
            self._logger.debug('%d bytes.', len(chunk))
