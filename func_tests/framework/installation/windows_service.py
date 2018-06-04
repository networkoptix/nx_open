from framework.installation.service import Service, ServiceStatus


class WindowsService(Service):
    def __init__(self, winrm, name):
        self.query = winrm.wmi_query(u'Win32_Service', {u'Name': name})

    def stop(self):
        self.query.invoke_method(u'StopService', {})

    def start(self):
        self.query.invoke_method(u'StartService', {})

    def status(self):
        instance = self.query.get_one()
        if instance[u'ProcessId'] == u'0':
            pid = None
            assert instance[u'State'] != u'Running'
        else:
            pid = int(instance[u'ProcessId'])
        is_running = instance[u'State'] == u'Running'
        return ServiceStatus(is_running, pid)
