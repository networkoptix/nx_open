from framework.installation.service import Service, ServiceStatus
from framework.os_access.windows_remoting import WinRM, wmi


class WindowsService(Service):
    def __init__(self, winrm, name):  # type: (WinRM, str) -> ...
        self._wmi_service = winrm.wmi.cls(u'Win32_Service').reference({u'Name': name})
        self._winrm = winrm
        self._name = name

    def __repr__(self):
        return '<WindowsService {} at {}>'.format(self._name, self._winrm)

    def stop(self, timeout_sec=None):
        try:
            self._wmi_service.invoke_method(u'StopService', {}, timeout_sec=timeout_sec)
        except wmi.WmiInvokeFailed.specific_cls(7):
            pass
        service = self._wmi_service.get()
        processes = list(self._winrm.wmi.cls(u'Win32_Process').enumerate({}))
        for process in processes:
            if process['CommandLine'] == service['PathName']:
                process.invoke_method(u'Terminate', {})

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
