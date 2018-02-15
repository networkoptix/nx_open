from __future__ import print_function

from textwrap import dedent


class SSHConfig(object):
    def __init__(self, ssh_dir):
        ssh_dir.mkdir(exist_ok=True)
        self.path = ssh_dir / 'config'
        self._connections_dir = ssh_dir / 'connections'
        self._connections_dir.mkdir(exist_ok=True)

    def reset(self):
        self.path.write_text(dedent(u'''
            PasswordAuthentication no
            StrictHostKeyChecking no
            UserKnownHostsFile /dev/null
            ControlMaster auto
            ControlMaster auto
            ControlPersist 10m
            ControlPath {connections_dir}/%r@%h:%p
        ''').lstrip().format(connections_dir=self._connections_dir))

    def add_host(self, hostname, alias=None, port=None, user=None, key_path=None):
        lines = []
        if alias:
            lines.append(u'Host {}'.format(alias))
            lines.append(u'    HostName {}'.format(hostname))
        else:
            lines.append(u'Host {}'.format(hostname))
        if port:  # No problem if empty string.
            lines.append(u'    Port {}'.format(port))
        if user:  # No problem if empty string.
            lines.append(u'    User {}'.format(user))
        if key_path:  # No problem if empty string.
            lines.append(u'    IdentityFile {}'.format(key_path))
        with self.path.open('a') as config_file:
            config_file.write('\n'.join(lines) + '\n')
