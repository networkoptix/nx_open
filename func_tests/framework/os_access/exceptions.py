from pylru import lrudecorator


def _error_message_from_stderr(stderr):
    """Simple heuristic to get short message from STDERR"""
    for line in reversed(stderr.splitlines()):
        if line and not line.startswith('+'):  # Omit empty lines and lines from set -x.
            return line
    return 'stderr empty'


class NonZeroExitStatus(Exception):
    def __init__(self, exit_status, stdout, stderr):
        super(NonZeroExitStatus, self).__init__(
            "{}".format(_error_message_from_stderr(stderr)))
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


class AlreadyExists(Exception):
    pass


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
