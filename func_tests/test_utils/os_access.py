"""Unified layer of access to OS, locally or via SSH"""

import abc
import datetime
import errno
import glob
import logging
import shutil
import subprocess
import threading
from textwrap import dedent
try:
    from shlex import quote  # In Python 3.3+.
except ImportError:
    from pipes import quote  # In Python 2.7 but deprecated.

import pytz
import tzlocal
from pathlib2 import Path, PurePosixPath

from .utils import RunningTime

log = logging.getLogger(__name__)


PROCESS_TIMEOUT = datetime.timedelta(hours=1)


def _arg_list_to_str(args):
    return ' '.join(quote(str(arg)) for arg in args)


def args_from_env(env):
    env = env or dict()
    return ['{}={}'.format(name, quote(str(value))) for name, value in env.items()]


class ProcessError(subprocess.CalledProcessError):
    def __init__(self, return_code, args, output):
        subprocess.CalledProcessError.__init__(self, return_code, _arg_list_to_str(args), output=output)

    def __str__(self):
        return 'Command "%s" returned non-zero exit status %d:\n%s' % (self.cmd, self.returncode, self.output)

    def __repr__(self):
        return 'ProcessError(%s)' % self


class ProcessTimeoutError(subprocess.CalledProcessError):

    def __init__(self, args, timeout):
        subprocess.CalledProcessError.__init__(self, returncode=None, cmd=_arg_list_to_str(args))
        self.timeout = timeout

    def __str__(self):
        return 'Command "%s" was timed out (timeout is %s)' % (self.cmd, self.timeout)

    def __repr__(self):
        return 'ProcessTimeoutError(%s)' % self


def log_output(name, output):
    if not output:
        return  # do not log ''; skip None also
    if '\0' in output:
        log.debug('\t--> %s: %s bytes binary', name, len(output))
    elif len(output) > 200:
        log.debug('\t--> %s: %r...', name, output[:200])
    else:
        log.debug('\t--> %s: %r',  name, output.rstrip('\r\n'))


class SshAccessConfig(object):

    @classmethod
    def from_dict(cls, d):
        assert isinstance(d, dict), 'ssh location should be dict: %r'  % d
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
        return '<os_access=%r user=%r key_file=%r>' % (self.hostname, self.user, self.key_file_path)


class OsAccess(object):

    __metaclass__ = abc.ABCMeta

    @abc.abstractmethod
    def run_command(self, args, input=None, cwd=None, check_retcode=True, log_output=True, timeout=None, env=None):
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

    def run_command(self, args, input=None, cwd=None, check_retcode=True, log_output=True, timeout=None, env=None):
        timeout = timeout or PROCESS_TIMEOUT
        args = [str(arg) for arg in args]
        if input:
            log.debug('executing: %s (with %d bytes input)', _arg_list_to_str(args), len(input))
            stdin = subprocess.PIPE
        else:
            log.debug('executing: %s', _arg_list_to_str(args))
            stdin = None
        pipe = subprocess.Popen(
            args, cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            stdin=stdin,
            close_fds=True,
            env=env,
            )
        stdout_buffer = []
        stderr_buffer = []
        stdout_thread = threading.Thread(target=self._read_thread, args=(log.debug, pipe.stdout, stdout_buffer, log_output))
        stderr_thread = threading.Thread(target=self._read_thread, args=(log.error, pipe.stderr, stderr_buffer, True))
        stdout_thread.daemon = True
        stderr_thread.daemon = True
        stdout_thread.start()
        stderr_thread.start()
        try:
            try:
                if input:
                    try:
                        pipe.stdin.write(input)
                    except IOError as e:
                        if e.errno == errno.EPIPE:
                            # communicate() should ignore broken pipe error
                            pass
                        elif e.errno == errno.EINVAL and pipe.poll() is not None:
                            # stdin.write() fails with EINVAL if the process already exited before the write
                            pass
                        else:
                            raise
                    pipe.stdin.close()
            finally:
                stdout_thread.join(timeout=timeout.total_seconds())
                if not stdout_thread.isAlive():
                    stderr_thread.join(timeout=timeout.total_seconds())
                if stdout_thread.isAlive() or stderr_thread.isAlive():
                    pipe.kill()
                    raise ProcessTimeoutError(args, timeout)
        finally:
            stdout_thread.join()
            stderr_thread.join()
            pipe.stdout.close()
            pipe.stderr.close()
        retcode = pipe.wait()
        if check_retcode and retcode:
            raise ProcessError(retcode, args, output=''.join(stderr_buffer))
        return ''.join(stdout_buffer)

    def _read_thread(self, logger, f, buffer, log_lines):
        is_binary = False
        while True:
            line = f.readline()
            if not line:
                return
            if '\0' in line:
                is_binary = True
            if log_lines and not is_binary:
                logger('\t> %s' % ''.join(map(self._mask_ws, line.rstrip('\r\n'))))
            buffer.append(line)

    def _mask_ws(self, ch):
        if ch == '\t' or ch >= ' ':
            return ch
        else:
            return '.'

    def file_exists(self, path):
        return path.exists()

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
        with from_remote_path.open('rb') as f:
            if ofs:
                f.seek(ofs)
            return f.read()

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
        shutil.rmtree(str(path), ignore_errors=ignore_errors)

    def expand_path(self, path):
        return Path(path).expanduser().resolve()

    def expand_glob(self, path_mask, for_shell=False):
        return [Path(path) for path in glob.glob(str(path_mask))]

    def get_timezone(self):
        return tzlocal.get_localzone()


class SshAccess(OsAccess):

    def __init__(self, config_path, hostname, user=None):
        self.hostname = hostname
        self._config_path = config_path
        self._user_and_hostname = user + '@' + hostname if user else hostname

    def __repr__(self):
        args = ['ssh', '-F', self._config_path, self._user_and_hostname]
        return '<{} {}>'.format(self.__class__.__name__, _arg_list_to_str(args))

    def become(self, user):
        other_user_access = self.__class__(self._config_path, self.hostname, user=user)
        try:
            other_user_access.run_command(':')
        except ProcessError:
            self.run_command(['sudo', 'cp', '-r', '/home/vagrant/.ssh', '~{}/'.format(user)])
        return other_user_access

    def run_command(self, args, input=None, cwd=None, check_retcode=True, log_output=True, timeout=None, env=None):
        if isinstance(args, str):
            script = dedent(args).strip()
            script = '\n'.join(args_from_env(env)) + '\n' + script
            if cwd:
                script = 'cd "{}"\n'.format(cwd) + script
            args = ['\n' + script]
        else:
            args = [str(arg) for arg in args]
            args = args_from_env(env) + args
            if cwd:
                args = ['cd', str(cwd), ';'] + args
        return LocalAccess().run_command(
            ['ssh', '-F', self._config_path, self._user_and_hostname] + args,
            input,
            check_retcode=check_retcode,
            log_output=log_output,
            timeout=timeout)

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
        LocalAccess().run_command(['rsync', '-aL', '-e', _arg_list_to_str(ssh_args), source, destination])

    def read_file(self, from_remote_path, ofs=None):
        if ofs:
            assert type(ofs) is int, repr(ofs)
            return self.run_command(['tail', '--bytes=+%d' % ofs, from_remote_path], log_output=False)
        else:
            return self.run_command(['cat', from_remote_path], log_output=False)

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
        self.run_command(['rm', '-r', path], check_retcode=not ignore_errors)

    def expand_path(self, path):
        assert '*' not in path, repr(path)  # this function is only for expanding ~ and $VARs. for globs use expand_glob
        return self.run_command(['echo', path]).strip()

    def expand_glob(self, path_mask, for_shell=False):
        if not path_mask.is_absolute():
            raise ValueError("Path mask must be absolute, not %r", path_mask)
        if for_shell:
            return [str(path_mask)]  # ssh expands '*' masks automatically
        else:
            fixed_root = next(parent for parent in path_mask.parent.parents if '*' not in str(parent))
            output = self.run_command(['find', fixed_root, '-wholename',  "'" + str(path_mask) + "'"]).strip()
            return [PurePosixPath(path) for path in output.splitlines()]

    def get_timezone(self):
        tzname = self.read_file('/etc/timezone').strip()
        return pytz.timezone(tzname)
