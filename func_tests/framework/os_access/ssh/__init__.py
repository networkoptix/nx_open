from textwrap import dedent

import pytz
import subprocess
from pathlib2 import Path, PurePosixPath

from framework.os_access import FileNotFound, NonZeroExitStatus, OsAccess, args_to_command
from framework.os_access.args import env_to_args
from framework.os_access.local import LocalAccess

SSH_CONFIG_PATH = Path(__file__).with_name('config')


class SSHAccess(OsAccess):
    def __init__(self, hostname, port, username='root', private_key_path=None):
        self.username = username
        self.hostname = hostname
        self.port = port
        self.private_key_path = private_key_path
        self._username_and_hostname = self.username + '@' + self.hostname
        self._private_key_args = ['-F', SSH_CONFIG_PATH]
        if self.private_key_path:
            self._private_key_args = ['-i', self.private_key_path]
        self._args = ['ssh', '-p', self.port] + self._private_key_args

    def __repr__(self):
        return '<SSHAccess {} {}>'.format(args_to_command(self._args), self._username_and_hostname)

    def run_command(self, args, input=None, cwd=None, timeout=None, env=None):
        if isinstance(args, str):
            script = dedent(args).strip()
            script = '\n'.join(env_to_args(env)) + '\n' + script
            if cwd:
                script = 'cd "{}"\n'.format(cwd) + script
            script = "set -euxo pipefail\n" + script
            prepared_args = ['\n' + script]
        else:
            args = [str(arg) for arg in args]
            args = env_to_args(env) + args
            if cwd:
                args = ['cd', str(cwd), ';'] + args
            prepared_args = [subprocess.list2cmdline(args)]
        return LocalAccess().run_command(self._args + [self._username_and_hostname] + prepared_args, input)

    def is_working(self):
        try:
            self.run_command([':'])
        except NonZeroExitStatus:
            return False
        return True

    @staticmethod
    def home():
        return PurePosixPath('~')

    def file_exists(self, path):
        output = self.run_command(['[', '-f', path, ']', '&&', 'echo', 'yes', '||', 'echo', 'no']).strip()
        assert output in ['yes', 'no'], repr(output)
        return output == 'yes'

    def dir_exists(self, path):
        output = self.run_command(['[', '-d', path, ']', '&&', 'echo', 'yes', '||', 'echo', 'no']).strip()
        assert output in ['yes', 'no'], repr(output)
        return output == 'yes'

    def put_file(self, from_local_path, to_remote_path):
        self._call_rsync(from_local_path, '{}:{}'.format(self._username_and_hostname, to_remote_path))

    def get_file(self, from_remote_path, to_local_path):
        self._call_rsync('{}:{}'.format(self._username_and_hostname, from_remote_path), to_local_path)

    def _call_rsync(self, source, destination):
        # If file size and modification time is same, rsync doesn't copy file.
        LocalAccess().run_command(['rsync', '-aL', '-e', args_to_command(self._args), source, destination])

    def read_file(self, from_remote_path, ofs=None):
        try:
            if ofs:
                assert type(ofs) is int, repr(ofs)
                return self.run_command(['tail', '--bytes=+%d' % ofs, from_remote_path])
            else:
                return self.run_command(['cat', from_remote_path])
        except NonZeroExitStatus as e:
            if e.exit_status == 1:
                if not self.file_exists(from_remote_path):
                    raise FileNotFound(from_remote_path)
            raise

    def write_file(self, to_remote_path, contents):
        to_remote_path = PurePosixPath(to_remote_path)
        self.run_command(['mkdir', '-p', to_remote_path.parent, '&&', 'cat', '>', to_remote_path], contents)

    def get_file_size(self, path):
        return self.run_command(['stat', '--format=%s', path])

    def mk_dir(self, path):
        self.run_command(['mkdir', '-p', str(path)])

    def iter_dir(self, path):
        return [path / line for line in self.run_command(['ls', '-A', '-1', str(path)]).splitlines()]

    def rm_tree(self, path, ignore_errors=False):
        try:
            self.run_command(['rm', '-r', path])
        except NonZeroExitStatus:
            if not ignore_errors:
                raise

    def expand_path(self, path):
        assert '*' not in str(path), repr(path)  # Only expands user and env vars.
        return PurePosixPath(self.run_command(['echo', path]).strip())

    def expand_glob(self, path_mask, for_shell=False):
        if not path_mask.is_absolute():
            raise ValueError("Path mask must be absolute, not %r", path_mask)
        if for_shell:
            return [str(path_mask)]  # ssh expands '*' masks automatically
        else:
            fixed_root = next(parent for parent in path_mask.parent.parents if '*' not in str(parent))
            output = self.run_command(['find', fixed_root, '-wholename', "'" + str(path_mask) + "'"]).strip()
            return [PurePosixPath(path) for path in output.splitlines()]

    def get_timezone(self):
        tzname = self.read_file('/etc/timezone').strip()
        return pytz.timezone(tzname)
