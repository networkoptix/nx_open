from abc import ABCMeta, abstractmethod, abstractproperty
from textwrap import dedent

from netaddr import EUI, IPAddress
from pathlib2 import PurePath
from six.moves import shlex_quote

from framework.os_access.command import Command, CommandOutcome, DEFAULT_RUN_TIMEOUT_SEC
from framework.os_access.path import copy_file_using_read_and_write

STREAM_BUFFER_SIZE = 16 * 1024
_PROHIBITED_ENV_NAMES = {'PATH', 'HOME', 'USER', 'SHELL', 'PWD', 'TERM'}


def quote_arg(arg):
    return shlex_quote(str(arg))


def command_to_script(command):
    return ' '.join(quote_arg(str(arg)) for arg in command)


def env_values_to_str(env):
    converted_env = {}
    for name, value in env.items():
        if isinstance(value, bool):  # Beware: bool is subclass of int.
            converted_env[name] = 'true' if value else 'false'
            continue
        if isinstance(value, (int, PurePath, IPAddress, EUI)):
            converted_env[name] = str(value)
            continue
        if isinstance(value, str):
            converted_env[name] = value
            continue
        if value is None:
            converted_env[name] = ''
            continue
        raise RuntimeError("Unexpected value {!r} of type {}".format(value, type(value)))
    return converted_env


def env_to_command(env):
    converted_env = env_values_to_str(env)
    command = []
    for name, value in converted_env.items():
        if name in _PROHIBITED_ENV_NAMES:
            raise ValueError("Potential name clash with built-in name: {}".format(name))
        command.append('{}={}'.format(name, quote_arg(str(value))))
    return command


def augment_script(script, cwd=None, env=None, set_eux=True, shebang=False):
    augmented_script_lines = []
    if shebang:
        # language=Bash
        augmented_script_lines.append('#!/bin/sh')
    if set_eux:
        # language=Bash
        augmented_script_lines.append('set -eux')  # It's sh (dash), pipefail cannot be set here.
    if cwd is not None:
        augmented_script_lines.append(command_to_script(['cd', cwd]))
    if env is not None:
        augmented_script_lines.extend(env_to_command(env))
    augmented_script_lines.append(dedent(script).strip())
    augmented_script = '\n'.join(augmented_script_lines)
    return augmented_script


class Outcome(CommandOutcome):
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


class Shell(object):
    """Posix-specific interface"""
    __metaclass__ = ABCMeta

    @abstractmethod
    def command(self, args, cwd=None, env=None, logger=None):
        return Command()

    @abstractmethod
    def sh_script(self, script, cwd=None, env=None, logger=None, set_eux=True):
        return Command()

    def run_command(self, args, input=None, cwd=None, logger=None, timeout_sec=DEFAULT_RUN_TIMEOUT_SEC, env=None):
        """Shortcut. Deprecated."""
        return self.command(
            args, cwd=cwd, env=env, logger=logger).check_output(input=input, timeout_sec=timeout_sec)

    def run_sh_script(self, script, input=None, cwd=None, logger=None, timeout_sec=DEFAULT_RUN_TIMEOUT_SEC, env=None):
        """Shortcut. Deprecated."""
        return self.sh_script(
            script, cwd=cwd, env=env, logger=logger).check_output(input=input, timeout_sec=timeout_sec)

    def copy_posix_file_to(self, posix_source, destination):
        copy_file_using_read_and_write(posix_source, destination)

    def copy_file_from_posix(self, source, posix_destination):
        copy_file_using_read_and_write(source, posix_destination)
