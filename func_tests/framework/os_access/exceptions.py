from pylru import lrudecorator


def _error_message_from_stderr(stderr):
    """Simple heuristic to get short message from STDERR: take last lines that doesn't begin with
    plus `+` (command trace). Some lines may be continuation lines which start with space or tab.
    First line of result is non-continuation, others are continuation.

    `stderr` decoded as UTF-8 because shell sometimes shows quotation mark (`b'\xe2\x80\x98'`).
    """
    lines = []
    for line in stderr.decode('utf-8').splitlines():
        if line and not line.startswith('+'):  # Omit empty lines and lines from set -x.
            if not line.startswith(' ') and not line.startswith('\t'):
                lines = []
            lines.append(line)
    return '\n'.join(lines) if lines else 'stderr empty'


class NonZeroExitStatus(Exception):
    def __init__(self, exit_status, stdout, stderr):
        super(NonZeroExitStatus, self).__init__(
            u"{}".format(_error_message_from_stderr(stderr)))
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


class DoesNotExist(Exception):
    pass


class CannotDownload(DoesNotExist):
    pass


class AlreadyExists(Exception):
    def __init__(self, message, path=None):
        super(AlreadyExists, self).__init__(message)
        self.path = path


class BadParent(Exception):
    pass


class NotADir(Exception):
    pass


class NotAFile(Exception):
    pass


class FileIsADir(Exception):
    pass


class DirIsAFile(Exception):
    pass


class AlreadyAcquired(Exception):
    pass


class CoreDumpError(Exception):

    def __init__(self, cause):
        super(CoreDumpError, self).__init__('Failed to make core dump: %s' % cause)
        self.cause = cause
