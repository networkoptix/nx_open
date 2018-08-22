import logging
import socket
from contextlib import closing

_logger = logging.getLogger(__name__)


def _check_tcp_port(address, port):
    with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as sock:
        try:
            sock.connect((address, port))
        except socket.error as e:
            _logger.warning("TCP connection to %s:%d: %r", address, port, e)
            return False
        _logger.info("TCP connection to %s:%d: success", address, port)
        return True


def _check_udp_port(address, port):
    with closing(socket.socket(socket.AF_INET, socket.SOCK_DGRAM)) as sock:
        try:
            # If `sock.connect` has been called, second `sock.send`
            # usually fails because of ICMP message "Host unreachable".
            # See also: https://stackoverflow.com/a/51296247/1833960
            _logger.debug("UDP connection to %s:%d: connect", address, port)
            sock.connect((address, port))
            for attempt in range(1, 10 + 1):
                _logger.debug("UDP connection to %s:%d: send attempt %d", address, port, attempt)
                sock.send(b'anything')
        except socket.error as e:
            _logger.warning("UDP connection and sending to %s:%d: %r", address, port, e)
            return False
        _logger.info("UDP connection and sending to %s:%d: success", address, port)
        return True


def port_is_open(protocol, address, port):
    if protocol == 'tcp':
        return _check_tcp_port(address, port)
    if protocol == 'udp':
        return _check_udp_port(address, port)
    raise ValueError("Protocol is either 'tcp' or 'udp', given {!r}".format(protocol))
