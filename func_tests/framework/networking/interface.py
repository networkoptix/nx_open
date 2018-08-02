from abc import ABCMeta, abstractmethod, abstractproperty

from netaddr import IPNetwork


class Networking(object):
    __metaclass__ = ABCMeta

    @abstractproperty
    def interfaces(self):
        pass

    @abstractmethod
    def reset(self):
        pass

    @abstractmethod
    def setup_ip(self, mac, ip, prefix_length):
        pass

    def setup_network(self, mac, network):
        ip_network = IPNetwork(network)
        self.setup_ip(mac, ip_network.ip, ip_network.prefixlen)

    @abstractmethod
    def route(self, destination_ip_net, gateway_bound_mac, gateway_ip):
        pass

    @abstractmethod
    def enable_internet(self):
        pass

    @abstractmethod
    def disable_internet(self):
        pass

    @abstractmethod
    def setup_nat(self, outer_mac):
        pass

    @abstractmethod
    def can_reach(self, ip, timeout_sec):
        pass
