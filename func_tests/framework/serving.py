import contextlib
import errno
import socket
import threading
import wsgiref.simple_server


def _reserve_port(ports_range, bind):
    for port in ports_range:
        try:
            result = bind(port)
        except socket.error as e:
            if e.errno == errno.EADDRINUSE:
                continue
            raise
        return port, result
    raise RuntimeError("Cannot find available port.")


@contextlib.contextmanager
def reserved_port(port_range):
    """Protect port from being used"""
    s = socket.socket()

    def bind(port):
        s.bind(('localhost', port))

    port, _ = _reserve_port(port_range, bind)
    with contextlib.closing(s):
        yield port


class WsgiServer(object):
    def __init__(self, app, port_range):
        def make_server(port):
            return wsgiref.simple_server.make_server('localhost', port, app)

        self.port, self._server = _reserve_port(port_range, make_server)

    @contextlib.contextmanager
    def serving(self):
        thread = threading.Thread(target=self._server.serve_forever)
        thread.start()
        try:
            yield
        finally:
            self._server.shutdown()
            thread.join()
            self._server.server_close()


def make_base_url_for_remote_machine(os_access, local_port):
    """Convenience function for fixtures"""
    # TODO: Refactor to get it out of here.
    port_map_local = os_access.port_map.local
    address_from_there = port_map_local.address
    port_from_there = port_map_local.tcp(local_port)
    url = 'http://{}:{}'.format(address_from_there, port_from_there)
    return url
