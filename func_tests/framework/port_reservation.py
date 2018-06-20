import errno
import socket


def reserve_port(ports_range, bind):
    for port in ports_range:
        try:
            result = bind(port)
        except socket.error as e:
            if e.errno == errno.EADDRINUSE:
                continue
            raise
        return port, result
    raise RuntimeError("Cannot find available port.")
