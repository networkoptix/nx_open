class WindowsService(object):
    def __init__(self, winrm, name):
        self.query = winrm.wmi_query('Win32_Service', {'Name': name})

    def stop(self):
        self.query.invoke_method('StopService', {})

    def start(self):
        self.query.invoke_method('StartService', {})

    def is_running(self):
        instance = self.query.get_one()
        return instance[u'State'] == u'Running'
