"""Unified layer of access to OS, locally or via SSH"""

import abc
import datetime
import errno
import glob
import logging
import shutil
import subprocess
from textwrap import dedent

try:
    from shlex import quote  # In Python 3.3+.
except ImportError:
    from pipes import quote  # In Python 2.7 but deprecated.

import pytz
import tzlocal
from pathlib2 import Path, PurePosixPath

from framework.communicate import communicate
from framework.utils import RunningTime

log = logging.getLogger(__name__)


def args_from_env(env):
    env = env or dict()
    return ['{}={}'.format(name, quote(str(value))) for name, value in env.items()]


def args_list_to_str(args):
    if isinstance(args, str):
        return args
    return ' '.join(quote(str(arg)) for arg in args)


class NonZeroExitStatus(Exception):
    def __init__(self, command, exit_status, stdout, stderr):
        self.exit_status = exit_status
        self.command = command
        self.stdout = stdout
        self.stderr = stderr

    def __str__(self):
        return "Command %s returned non-zero exit status %d" % (args_list_to_str(self.command), self.exit_status)


class Timeout(Exception):
    def __init__(self, command, timeout_sec):
        self.command = command
        self.timeout_sec = timeout_sec

    def __str__(self):
        return "Command %s was timed out (%s sec)" % (args_list_to_str(self.command), self.timeout_sec)


class FileNotFound(Exception):
    def __init__(self, path):
        super(FileNotFound, self).__init__("File {} not found".format(path))


class SshAccessConfig(object):
    @classmethod
    def from_dict(cls, d):
        assert isinstance(d, dict), 'ssh location should be {}: %r' % d
        assert 'host' in d, '"host" is required parameter for host location'
        return cls(
            name=d.get('name') or d['hostname'],
            hostname=d['hostname'],
            user=d.get('user'),
            key_file_path=d.get('key_file'),
            )

    def __init__(self, name, hostname, user=None, key_file_path=None):
        self.name = name
        self.hostname = hostname
        self.user = user
        self.key_file_path = key_file_path

    def __repr__(self):
        return '<hostname=%r user=%r key_file=%r>' % (self.hostname, self.user, self.key_file_path)


class OsAccess(object):
    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def run_command(self, args, input=None, cwd=None, env=None):
        pass

    @abc.abstractmethod
    def is_working(self):
        pass

    @abc.abstractmethod
    def file_exists(self, path):
        pass

    @abc.abstractmethod
    def dir_exists(self, path):
        pass

    @abc.abstractmethod
    def put_file(self, from_local_path, to_remote_path):
        pass

    @abc.abstractmethod
    def get_file(self, from_remote_path, to_local_path):
        pass

    @abc.abstractmethod
    def read_file(self, from_remote_path, ofs=None):
        pass

    @abc.abstractmethod
    def write_file(self, to_remote_path, contents):
        pass

    @abc.abstractmethod
    def get_file_size(self, path):
        pass

    # create parent dirs as well
    @abc.abstractmethod
    def mk_dir(self, path):
        pass

    @abc.abstractmethod
    def iter_dir(self, path):
        pass

    @abc.abstractmethod
    def rm_tree(self, path, ignore_errors=False):
        pass

    @abc.abstractmethod
    def expand_path(self, path):
        pass

    # expand path with '*' mask into list of paths
    # if for_shell=True, returns path_mask as is for remote ssh host -
    #   it is assumed to be used in shell command then, and shell will expand it by itself
    @abc.abstractmethod
    def expand_glob(self, path_mask, for_shell=False):
        pass

    @abc.abstractmethod
    def get_timezone(self):
        pass

    def set_time(self, new_time):
        started_at = datetime.datetime.now(pytz.utc)
        self.run_command(['date', '--set', new_time.isoformat()])
        return RunningTime(new_time, datetime.datetime.now(pytz.utc) - started_at)


class LocalAccess(OsAccess):
    def __init__(self):
        self.name = 'localhost'
        self.hostname = 'localhost'

    def __repr__(self):
        return '<LocalAccess>'

    def run_command(self, command, input=None, cwd=None, env=None, timeout_sec=60 * 60):
        if isinstance(command, (bytes, str)):
            command = 'set -eux\n' + dedent(command).strip()
            shell = True
        else:
            command = [str(arg) for arg in command]
            shell = False
        cwd = cwd and str(cwd) or None
        env = env and {k: str(v) for k, v in env.items()} or None
        log.debug('Run: %s', args_list_to_str(command))
        process = subprocess.Popen(
            command,
            bufsize=16 * 1024 * 1024,
            shell=shell, cwd=cwd, env=env, close_fds=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE if input else None)
        exit_status, stdout, stderr = communicate(process, input, timeout_sec)
        if exit_status is None:
            raise Timeout(command, timeout_sec)
        if exit_status != 0:
            raise NonZeroExitStatus(command, exit_status, stdout, stderr)
        return stdout

    def is_working(self):
        return True

    @staticmethod
    def home():
        return Path.home()

    def file_exists(self, path):
        return Path(path).exists()

    def dir_exists(self, path):
        return path.is_dir()

    def put_file(self, from_local_path, to_remote_path):
        self._copy(from_local_path, to_remote_path)

    def get_file(self, from_remote_path, to_local_path):
        self._copy(from_remote_path, to_local_path)

    def _copy(self, from_path, to_path):
        log.debug('copying %s -> %s', from_path, to_path)
        shutil.copy2(str(from_path), str(to_path))

    def read_file(self, from_remote_path, ofs=None):
        try:
            with from_remote_path.open('rb') as f:
                if ofs:
                    f.seek(ofs)
                return f.read()
        except IOError as e:
            if e.errno == errno.ENOENT:
                raise FileNotFound(from_remote_path)
            raise

    def write_file(self, to_remote_path, contents):
        log.debug('writing %d bytes to %s', len(contents), to_remote_path)
        self.mk_dir(to_remote_path.parent)
        to_remote_path.write_bytes(contents)

    def get_file_size(self, path):
        return path.stat().st_size

    def mk_dir(self, path):
        path.mkdir(parents=True, exist_ok=True)

    def iter_dir(self, path):
        return path.iterdir()

    def rm_tree(self, path, ignore_errors=False):
        if path.is_file():
            path.unlink()
        else:
            shutil.rmtree(str(path), ignore_errors=ignore_errors)

    def expand_path(self, path):
        return Path(path).expanduser().resolve()

    def expand_glob(self, path_mask, for_shell=False):
        return [Path(path) for path in glob.glob(str(path_mask))]

    def get_timezone(self):
        return tzlocal.get_localzone()


class SSHAccess(OsAccess):
    def __init__(self, config_path, hostname, user=None):
        self.hostname = hostname
        self._config_path = config_path
        self._user_and_hostname = user + '@' + hostname if user else hostname

    def __repr__(self):
        args = ['ssh', '-F', self._config_path, self._user_and_hostname]
        return '<{} {}>'.format(self.__class__.__name__, args_list_to_str(args))

    def run_command(self, args, input=None, cwd=None, timeout=None, env=None):
        if isinstance(args, str):
            script = dedent(args).strip()
            script = '\n'.join(args_from_env(env)) + '\n' + script
            if cwd:
                script = 'cd "{}"\n'.format(cwd) + script
            script = "set -euxo pipefail\n" + script
            args = ['\n' + script]
        else:
            args = [str(arg) for arg in args]
            args = args_from_env(env) + args
            if cwd:
                args = ['cd', str(cwd), ';'] + args
        return LocalAccess().run_command(['ssh', '-F', self._config_path, self._user_and_hostname] + args, input)

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
        self._call_rsync(from_local_path, '{}:{}'.format(self._user_and_hostname, to_remote_path))

    def get_file(self, from_remote_path, to_local_path):
        self._call_rsync('{}:{}'.format(self._user_and_hostname, from_remote_path), to_local_path)

    def _call_rsync(self, source, destination):
        # If file size and modification time is same, rsync doesn't copy file.
        ssh_args = ['ssh', '-F', self._config_path]  # Without user and hostname!
        LocalAccess().run_command(['rsync', '-aL', '-e', args_list_to_str(ssh_args), source, destination])

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
