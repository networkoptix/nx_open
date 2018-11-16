from pylru import lrudecorator


def _error_message_from_output(stdout, stderr):
    """Simple heuristic to get short message from output: take last lines that doesn't begin with
    plus `+` (command trace). Some lines may be continuation lines which start with space or tab.
    First line of result is non-continuation, others are continuation.

    `stderr` decoded as UTF-8 because shell sometimes shows quotation mark (`b'\xe2\x80\x98'`).
    """
    for name, output in (('stderr', stderr), ('stdout', stdout)):
        if len(output) > 100 * 1024:
            return name + ' bigger than 100 kilobytes'
        lines = []
        for line in output.decode('utf-8').splitlines():
            if line and not line.startswith('+'):  # Omit empty lines and lines from set -x.
                if not line.startswith(' ') and not line.startswith('\t'):
                    lines = []
                lines.append(line)
        if lines:
            return '\n'.join(lines)
    return 'no output'


class NonZeroExitStatus(Exception):
    def __init__(self, exit_status, stdout, stderr):
        super(NonZeroExitStatus, self).__init__(
            u"{}".format(_error_message_from_output(stdout, stderr)))
        self.exit_status = exit_status
        self.stdout = stdout
        self.stderr = stderr


@lrudecorator(255)
def exit_status_error_cls(exit_status):
    assert exit_status != 0

    class SpecificExitStatus(NonZeroExitStatus):
        def __init__(self, stdout, stderr):
            super(SpecificExitStatus, self).__init__(exit_status, stdout, stderr)

    return type('ExitStatus{:d}'.format(exit_status), (SpecificExitStatus,), {})


class Timeout(Exception):
    def __init__(self, timeout_sec):
        super(Timeout, self).__init__("Process was timed out ({} sec)".format(timeout_sec))
        self.timeout_sec = timeout_sec


class FileSystemError(OSError):
    def __init__(self, errno, strerror):  # type: (int, str) -> ...
        super(FileSystemError, self).__init__(errno, strerror)


class BadPath(FileSystemError):
    pass


class DoesNotExist(BadPath):
    pass


class CannotDownload(Exception):
    pass


class AlreadyExists(FileSystemError):
    pass


class BadParent(FileSystemError):
    pass


class NotADir(BadPath):
    pass


class NotAFile(BadPath):
    pass


class NotEmpty(FileSystemError):
    pass


class AlreadyAcquired(Exception):
    pass


class CoreDumpError(Exception):

    def __init__(self, cause):
        super(CoreDumpError, self).__init__('Failed to make core dump: %s' % cause)
        self.cause = cause
