from framework.installation.service import Service, ServiceStatus
from framework.os_access.windows_remoting import WinRM, wmi


class WindowsService(Service):
    def __init__(self, winrm, name):  # type: (WinRM, str) -> ...
        self._wmi_service = winrm.wmi_class(u'Win32_Service').reference({u'Name': name})
        self._winrm = winrm
        self._name = name

    def __repr__(self):
        return '<WindowsService {} at {}>'.format(self._name, self._winrm)

    def stop(self, timeout_sec=None):
        pid = self.status().pid
        try:
            self._wmi_service.invoke_method(u'StopService', {}, timeout_sec=timeout_sec)
        except wmi.WmiInvokeFailed.specific_cls(7):
            self._winrm.wmi_class(u'Win32_Process').reference({u'Handle': pid}).invoke_method(u'Terminate', {})

    def start(self, timeout_sec=None):
        self._wmi_service.invoke_method(u'StartService', {}, timeout_sec=timeout_sec)

    def status(self):
        instance = self._wmi_service.get()
        if instance[u'ProcessId'] == u'0':
            pid = None
            assert instance[u'State'] != u'Running'
        else:
            pid = int(instance[u'ProcessId'])
        is_running = instance[u'State'] == u'Running'
        return ServiceStatus(is_running, pid)
