"""Unified layer of access to OS, locally or via SSH"""

import abc
import datetime
import errno
import glob
import logging
import shutil
import subprocess
from select import select
from textwrap import dedent

from math import ceil

try:
    from shlex import quote  # In Python 3.3+.
except ImportError:
    from pipes import quote  # In Python 2.7 but deprecated.

import pytz
import tzlocal
from pathlib2 import Path, PurePosixPath

from .utils import RunningTime, Wait

log = logging.getLogger(__name__)


PROCESS_TIMEOUT = datetime.timedelta(hours=1)


def _arg_list_to_str(args):
    return ' '.join(quote(str(arg)) for arg in args)


def args_from_env(env):
    env = env or dict()
    return ['{}={}'.format(name, quote(str(value))) for name, value in env.items()]


class ProcessError(subprocess.CalledProcessError):
    def __init__(self, return_code, command, output):
        subprocess.CalledProcessError.__init__(self, return_code, command, output=output)

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


class FileNotFound(Exception):
    def __init__(self, path):
        super(FileNotFound, self).__init__("File {} not found".format(path))


class SshAccessConfig(object):

    @classmethod
    def from_dict(cls, d):
        assert isinstance(d, dict), 'ssh location should be dict: %r' % d
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
    def run_command(self, args, input=None, cwd=None, check_retcode=True, log_output=True, timeout=None, env=None):
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

    def run_command(self, args, input=None, cwd=None, check_retcode=True, log_output=True, timeout=None, env=None):
        timeout = timeout or PROCESS_TIMEOUT
        if isinstance(args, (bytes, str)):
            args = 'set -eux\n' + dedent(args).strip()
            shell = True
            logged_command_line = args
        else:
            args = [str(arg) for arg in args]
            shell = False
            logged_command_line = _arg_list_to_str(args)
        log.debug('Execute:\n%s', logged_command_line)
        pipe = subprocess.Popen(
            args,
            shell=shell,
            cwd=cwd and str(cwd), env=env and {k: str(v) for k, v in env.items()},
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE if input else None,
            close_fds=True)
        process_logger = log.getChild('subprocesses').getChild(str(pipe.pid))
        process_logger.debug("Started.")
        if input:
            try:
                process_logger.debug("STDIN data:\n%s", input)
                pipe.stdin.write(input)
            except IOError as e:
                if e.errno == errno.EPIPE:
                    process_logger.error("STDIN broken.")
                    pass
                elif e.errno == errno.EINVAL and pipe.poll() is not None:
                    if pipe.poll() is None:
                        process_logger.error("STDIN invalid but process is still running.")
                    else:
                        process_logger.error("STDIN invalid: process terminated.")
                    pass
                else:
                    raise
            pipe.stdin.close()
            process_logger.debug("STDIN closed.")
        streams = [pipe.stdout, pipe.stderr]
        buffers = {stream: bytearray() for stream in streams}
        names = {pipe.stdout: 'STDOUT', pipe.stderr: 'STDERR'}
        wait = Wait(
            name='for events from process',
            timeout_sec=timeout.total_seconds(),
            logging_levels=(logging.DEBUG, logging.ERROR))
        while streams:
            select_timeout_sec = int(ceil(wait.delay_sec))
            process_logger.debug("Wait for select with %r seconds", select_timeout_sec)
            streams_to_read, _, _ = select(streams, [], [], select_timeout_sec)
            if not streams_to_read:
                process_logger.debug("Nothing in streams.")
                if pipe.poll() is not None:
                    break
                if not wait.again():
                    raise ProcessTimeoutError(args, timeout)
            else:
                for stream in streams_to_read:
                    process_logger.debug("%s to be read from.", names[stream])
                    chunk = stream.read()
                    if not chunk:
                        process_logger.debug("%s to be closed, %d bytes total.", names[stream], len(buffers[stream]))
                        stream.close()
                        streams.remove(stream)
                    else:
                        if stream is not pipe.stdout or log_output:
                            try:
                                process_logger.debug("%s data to be decoded.")
                                decoded = chunk.decode()
                            except UnicodeDecodeError as e:
                                process_logger.debug(
                                    "%s data, %d bytes, considered binary because of %r.",
                                    names[stream], len(chunk), e)
                            else:
                                process_logger.debug(
                                    u"%s data, %d bytes, %d characters, decoded:\n%s",
                                    names[stream], len(chunk), len(decoded), decoded)
                        process_logger.debug("%s data appended to buffer.")
                        buffers[stream].extend(chunk)
        while True:
            return_code = pipe.poll()
            if return_code is None:
                if not wait.sleep_and_continue():
                    raise ProcessTimeoutError(args, timeout)
            else:
                break
        process_logger.debug("Return code %d.", return_code)
        if check_retcode and return_code:
            raise ProcessError(return_code, logged_command_line, output=bytes(buffers[pipe.stderr]))
        return bytes(buffers[pipe.stdout])

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
        return '<{} {}>'.format(self.__class__.__name__, _arg_list_to_str(args))

    def run_command(self, args, input=None, cwd=None, check_retcode=True, log_output=True, timeout=None, env=None):
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
        return LocalAccess().run_command(
            ['ssh', '-F', self._config_path, self._user_and_hostname] + args,
            input,
            check_retcode=check_retcode,
            log_output=log_output,
            timeout=timeout)

    def is_working(self):
        try:
            self.run_command([':'])
        except ProcessError:
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
        LocalAccess().run_command(['rsync', '-aL', '-e', _arg_list_to_str(ssh_args), source, destination])

    def read_file(self, from_remote_path, ofs=None):
        try:
            if ofs:
                assert type(ofs) is int, repr(ofs)
                return self.run_command(['tail', '--bytes=+%d' % ofs, from_remote_path], log_output=False)
            else:
                return self.run_command(['cat', from_remote_path], log_output=False)
        except ProcessError as e:
            if e.returncode == 1:
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
        self.run_command(['rm', '-r', path], check_retcode=not ignore_errors)

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
            output = self.run_command(['find', fixed_root, '-wholename',  "'" + str(path_mask) + "'"]).strip()
            return [PurePosixPath(path) for path in output.splitlines()]

    def get_timezone(self):
        tzname = self.read_file('/etc/timezone').strip()
        return pytz.timezone(tzname)
