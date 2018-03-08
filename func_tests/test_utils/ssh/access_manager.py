from test_utils.os_access import SSHAccess


class SSHAccessManager(object):
    guest_port = 22

    def __init__(self, ssh_config, user, key_path):
        self._ssh_config = ssh_config
        self._user = user
        self._key_path = key_path

    def register(self, hostname, aliases, port):
        self._ssh_config.add_host(hostname, aliases=aliases, port=port, user=self._user, key_path=self._key_path)
        return SSHAccess(self._ssh_config.path, aliases[0])