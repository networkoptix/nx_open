from framework.installation.service import Service, ServiceStatus


class WindowsService(Service):
    def __init__(self, winrm, name):
        self.query = winrm.wmi_query(u'Win32_Service', {u'Name': name})
        self._winrm = winrm
        self._name = name

    def __repr__(self):
        return '<WindowsService {} at {}>'.format(self._name, self._winrm)

    def stop(self, timeout_sec=None):
        self.query.invoke_method(u'StopService', {}, timeout_sec=timeout_sec)

    def start(self, timeout_sec=None):
        self.query.invoke_method(u'StartService', {}, timeout_sec=timeout_sec)

    def status(self):
        instance = self.query.get()
        if instance[u'ProcessId'] == u'0':
            pid = None
            assert instance[u'State'] != u'Running'
        else:
            pid = int(instance[u'ProcessId'])
        is_running = instance[u'State'] == u'Running'
        return ServiceStatus(is_running, pid)
