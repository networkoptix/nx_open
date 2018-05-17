import abc


class Service(object):
    __metaclass__ = abc.ABCMeta

    # returns bool: True if server is running, False if it's not
    @abc.abstractmethod
    def is_running(self):
        pass

    @abc.abstractmethod
    def start(self):
        pass

    @abc.abstractmethod
    def stop(self):
        pass
