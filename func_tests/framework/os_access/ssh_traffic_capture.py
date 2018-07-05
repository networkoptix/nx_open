from framework.os_access.ssh_shell import SSH
from framework.os_access.traffic_capture import TrafficCapture


class SSHTrafficCapture(TrafficCapture):
    def __init__(self, shell, dir):
        super(SSHTrafficCapture, self).__init__(dir)
        self.shell = shell  # type: SSH

    def _make_capturing_command(self, capture_path, size_limit_bytes, duration_limit_sec):
        return self.shell.terminal_command([
            'tshark',
            '-a', 'filesize:' + size_limit_bytes // 1024,
            '-a', 'duration:' + duration_limit_sec,
            '-w', capture_path,
            ])
