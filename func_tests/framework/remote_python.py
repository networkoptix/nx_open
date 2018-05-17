from framework.os_access.exceptions import NonZeroExitStatus


class RemotePython(object):
    def __init__(self, os_access):
        self._ssh_access = os_access

    def is_installed(self):
        try:
            self._ssh_access.run_command(['python3.4', '-V'])
        except NonZeroExitStatus:
            return False
        return True

    def install(self):
        # In Ubuntu 14.04 Vagrant box, Python 3.4 is installed but venv isn't.
        self._ssh_access.run_command([
            'if', '!', 'dpkg-query', '--show', 'python3.4-venv',
            '||', '!', 'dpkg-query', '--show', 'python3.4-dev', ';', 'then',
            'apt-get', 'update', ';',
            'apt-get', '--assume-yes', 'install',
            'python3.4-venv',
            'python3.4-dev',  # To build scandir.
            ';',
            'fi',
            ])

    def make_venv(self, path):
        venv_python_path = path / 'bin' / 'python'
        self._ssh_access.run_command([
            'test', '-e', venv_python_path,
            '||', 'python3.4', '-m', 'venv', path,
            '&&', venv_python_path, '-m', 'pip', 'install', '--upgrade', 'pip',  # Old PIP cannot install new Flask.
            '&&', venv_python_path, '-m', 'pip', 'install', '--upgrade', 'wheel',  # To bypass build.
            ])
        return venv_python_path
