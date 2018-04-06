from framework.networking.interface import Networking


class WindowsNetworking(Networking):
    def __init__(self, os_access, macs):
        super(WindowsNetworking, self).__init__()
        self._macs = macs
        self._os_access = os_access

    def can_reach(self, ip, timeout_sec):
        raise NotImplementedError()

    def route(self, destination_ip_net, gateway_bound_mac, gateway_ip):
        raise NotImplementedError()

    def setup_ip(self, mac, ip, prefix_length):
        raise NotImplementedError()

    def disable_internet(self):
        raise NotImplementedError()

    def enable_internet(self):
        raise NotImplementedError()

    def reset(self):
        raise NotImplementedError()

    @property
    def interfaces(self):
        raise NotImplementedError()

    def setup_nat(self, outer_mac):
        raise NotImplementedError()
