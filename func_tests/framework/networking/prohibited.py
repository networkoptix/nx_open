from framework.networking.interface import Networking


class ProhibitedNetworking(Networking):
    """Prohibit any changes, intended for physical (remote or local) machines"""

    @property
    def interfaces(self):
        raise NotImplementedError("Any changes in networking are prohibited on this machine")

    def reset(self):
        raise NotImplementedError("Any changes in networking are prohibited on this machine")

    def setup_ip(self, mac, ip, prefix_length):
        raise NotImplementedError("Any changes in networking are prohibited on this machine")

    def route(self, destination_ip_net, gateway_bound_mac, gateway_ip):
        raise NotImplementedError("Any changes in networking are prohibited on this machine")

    def enable_internet(self):
        raise NotImplementedError("Any changes in networking are prohibited on this machine")

    def disable_internet(self):
        raise NotImplementedError("Any changes in networking are prohibited on this machine")

    def setup_nat(self, outer_mac):
        raise NotImplementedError("Any changes in networking are prohibited on this machine")

    def can_reach(self, ip, timeout_sec):
        raise NotImplementedError("Any changes in networking are prohibited on this machine")
