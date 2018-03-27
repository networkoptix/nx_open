from __future__ import print_function

from pathlib2 import Path

from framework.os_access import SSHAccess


class SSHAccessManager(object):
    guest_port = 22

    def __init__(self, configuration_file_path, private_key_path):
        self.path = Path(configuration_file_path).expanduser()
        self.path.parent.mkdir(exist_ok=True)
        self._connections_dir = Path('/tmp/func_tests-ssh_connections').expanduser()
        self._connections_dir.mkdir(exist_ok=True)
        self._hosts = {}
        self._private_key_path = Path(private_key_path).expanduser()
        assert self._private_key_path.exists()

    def _write_configuration_file(self):
        with self.path.open('wb') as config_file:
            print('BatchMode', 'yes', file=config_file)
            print('EscapeChar', 'none', file=config_file)
            print('LogLevel', 'VERBOSE', file=config_file)
            print('StrictHostKeyChecking', 'no', file=config_file)
            print('UserKnownHostsFile', '/dev/null', file=config_file)
            print('ConnectTimeout', 5, file=config_file)
            print('ConnectionAttempts', 2, file=config_file)
            print('ControlMaster', 'auto', file=config_file)
            print('ControlPersist', '10m', file=config_file)
            print('ControlPath', self._connections_dir.resolve() / '%r@%h:%p.ssh.sock', file=config_file)
            print('User', 'root', file=config_file)
            print('IdentityFile', self._private_key_path.resolve(), file=config_file)
            for alias, (hostname, port) in self._hosts.items():
                print('Host', alias, file=config_file)
                print('    Hostname', hostname, file=config_file)
                print('    Port', port, file=config_file)

    def register(self, alias, hostname, port):
        self._hosts[alias] = hostname, port
        self._write_configuration_file()
        return SSHAccess(self.path, alias)

    def unregister(self, alias):
        del self._hosts[alias]
        self._write_configuration_file()


def make_access_manager(type, **settings):
    if type == 'ssh':
        return SSHAccessManager(**settings)
    raise NotImplementedError("No such access manager {!r}".format(type))
