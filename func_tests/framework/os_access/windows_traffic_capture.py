from framework.os_access.traffic_capture import TrafficCapture


class WindowsTrafficCapture(TrafficCapture):
    def __init__(self, dir, winrm):
        super(WindowsTrafficCapture, self).__init__(dir)
        self._winrm = winrm

    def _make_capturing_command(self, capture_path):
        size_limit_bytes = 500 * 1024 * 1024
        timeout_sec = 600
        command = self._winrm.command([
            'NMCap',
            '/CaptureProcesses', '/RecordFilters', '/RecordConfig',
            '/DisableLocalOnly',  # P-mode (promiscuous) to capture ICMP.
            '/Capture',  # `/Capture` is an action.
            # Here may come filter in Network Monitor language.
            '/Networks', '*',  # All network interfaces.
            '/File', '{}:{}'.format(capture_path, size_limit_bytes),  # File path and size limit.
            '/StopWhen', '/TimeAfter', timeout_sec, 'seconds',  # If teardown is not performed, stop at some moment.
            ])
        return command
