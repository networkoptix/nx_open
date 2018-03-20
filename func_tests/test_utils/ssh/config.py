from __future__ import print_function

from textwrap import dedent

from pathlib2 import Path


class SSHConfig(object):
    def __init__(self, path):
        self.path = path
        self._connections_dir = Path('/tmp/func_tests-ssh_connections')
        self._connections_dir.mkdir(exist_ok=True)

    def reset(self):
        self.path.write_text(dedent(u'''
            BatchMode yes
            EscapeChar none
            LogLevel VERBOSE
            StrictHostKeyChecking no
            UserKnownHostsFile /dev/null
            ConnectTimeout 5
            ConnectionAttempts 2
            ControlMaster auto
            ControlPersist 10m
            ControlPath {connections_dir}/%r@%h:%p.ssh.sock
        ''').lstrip().format(connections_dir=self._connections_dir.resolve()))

    def add_host(self, hostname, aliases=None, port=None, user=None, key_path=None):
        with self.path.open('a') as config_file:
            if aliases:
                config_file.write(u'Host {}\n'.format(' '.join(aliases)))
                config_file.write(u'    HostName {}\n'.format(hostname))
            else:
                config_file.write(u'Host {}\n'.format(hostname))
            if port and port != 22:  # No problem if empty string.
                config_file.write(u'    Port {}\n'.format(port))
            if user:  # No problem if empty string.
                config_file.write(u'    User {}\n'.format(user))
            if key_path:  # No problem if empty string.
                config_file.write(u'    IdentityFile {}\n'.format(key_path))
