from abc import ABCMeta, abstractmethod, abstractproperty

from netaddr import EUI, IPNetwork


class Networking(object):
    __metaclass__ = ABCMeta

    @abstractproperty
    def interfaces(self):
        pass

    @abstractmethod
    def reset(self):
        pass

    @abstractmethod
    def setup_network(self, mac, ip):
        # type: (EUI, IPNetwork) -> None
        pass

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

    @abstractmethod
    def is_router(self):  # type: () -> bool
        pass
