from abc import ABCMeta, abstractmethod, abstractproperty

from framework.os_access.command import Command, CommandOutcome

_DEFAULT_TIMEOUT_SEC = 60
_BIG_CHUNK_THRESHOLD_CHARS = 10000
_BIG_CHUNK_THRESHOLD_LINES = 50
_STREAM_BUFFER_SIZE = 16 * 1024


class PosixOutcome(CommandOutcome):
    __metaclass__ = ABCMeta

    # See: http://tldp.org/LDP/abs/html/exitcodes.html
    exit_statuses = {
        0: "Success",
        1: "Catchall for general errors: "
           "miscellaneous errors, such as \"divide by zero\" "
           "and other impermissible operations",
        2: "Misuse of shell builtins (according to Bash documentation): "
           "missing keyword or command, or permission problem "
           "(and `diff` return code on a failed binary file comparison)",
        126: "Command invoked cannot execute: "
             "permission problem or command is not an executable",
        127: "Command not found: "
             "possible problem with $PATH or a typo",
        128: "Exit status out of range: "
             "`exit` takes only integer args in the range 0 - 255"
        }

    # See: signal(7), table, middle of Value column -- http://man7.org/linux/man-pages/man7/signal.7.html
    signals = {
        1: ('SIGHUP', 'Term', 'Hangup detected on controlling terminal or death of controlling process'),
        2: ('SIGINT', 'Term', 'Interrupt from keyboard'),
        3: ('SIGQUIT', 'Core', 'Quit from keyboard'),
        4: ('SIGILL', 'Core', 'Illegal Instruction'),
        6: ('SIGABRT', 'Core', 'Abort signal from abort(3)'),
        8: ('SIGFPE', 'Core', 'Floating-point exception'),
        9: ('SIGKILL', 'Term', 'Kill signal'),
        11: ('SIGSEGV', 'Core', 'Invalid memory reference'),
        13: ('SIGPIPE', 'Term', 'Broken pipe: write to pipe with no readers; see pipe(7)'),
        14: ('SIGALRM', 'Term', 'Timer signal from alarm(2)'),
        15: ('SIGTERM', 'Term', 'Termination signal'),
        10: ('SIGUSR1', 'Term', 'User-defined signal 1'),
        12: ('SIGUSR2', 'Term', 'User-defined signal 2'),
        17: ('SIGCHLD', 'Ign', 'Child stopped or terminated'),
        18: ('SIGCONT', 'Cont', 'Continue if stopped'),
        19: ('SIGSTOP', 'Stop', 'Stop process'),
        20: ('SIGTSTP', 'Stop', 'Stop typed at terminal'),
        21: ('SIGTTIN', 'Stop', 'Terminal input for background process'),
        22: ('SIGTTOU', 'Stop', 'Terminal output for background process'),
        7: ('SIGBUS', 'Core', 'Bus error (bad memory access)'),
        27: ('SIGPROF', 'Term', 'Profiling timer expired'),
        31: ('SIGSYS', 'Core', 'Bad system call (SVr4); see also seccomp(2)'),
        5: ('SIGTRAP', 'Core'    'Trace/breakpoint trap'),
        23: ('SIGURG', 'Ign', 'Urgent condition on socket (4.2BSD)'),
        26: ('SIGVTALRM', 'Term', 'Virtual alarm clock (4.2BSD)'),
        24: ('SIGXCPU', 'Core', 'CPU time limit exceeded (4.2BSD); see setrlimit(2)'),
        25: ('SIGXFSZ', 'Core', 'File size limit exceeded (4.2BSD); see setrlimit(2)'),
        }

    @abstractproperty
    def signal(self):
        return 0

    @property
    def is_intended_termination(self):
        return self.signal == 2  # SIGINT

    @property
    def comment(self):
        if self.signal is None:
            try:
                message = self.exit_statuses[self.code]
            except KeyError:
                return "Exit status {}".format(self.code)
            return "Exit status {} ({})".format(self.code, message)
        try:
            name, action, comment = self.signals[self.signal]
        except KeyError:
            return "Terminated by unknown signal {}".format(self.signal)
        return "Terminated by {} ({}; default action: {})".format(name, comment, action)


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
        return self.command(args, cwd=cwd, env=env).check_output(input=input, timeout_sec=timeout_sec)

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
