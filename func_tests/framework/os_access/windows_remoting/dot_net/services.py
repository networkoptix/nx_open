from framework.os_access.windows_remoting.dot_net import get_cim_instance, invoke_cim_instance_method


class Service(object):
    class_name = 'Win32_Service'

    def __init__(self, protocol, name):
        self.protocol = protocol
        self.selectors = {'Name': name}

    def _invoke(self, method, params):
        return invoke_cim_instance_method(self.protocol, self.class_name, self.selectors, method, {})

    def stop(self):
        return self._invoke('StopService', {})

    def start(self):
        return self._invoke('StartService', {})

    def get(self):
        return get_cim_instance(self.protocol, self.class_name, self.selectors)
