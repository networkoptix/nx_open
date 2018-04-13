import errno
import glob
import logging
import shutil
import subprocess
from textwrap import dedent

import tzlocal
from pathlib2 import Path

from framework.os_access import FileNotFound, NonZeroExitStatus, OsAccess, Timeout, args_to_command
from framework.os_access.local.communicate import communicate

_logger = logging.getLogger(__name__)


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
        _logger.debug('Run: %s', args_to_command(command))
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
        _logger.debug('copying %s -> %s', from_path, to_path)
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
        _logger.debug('writing %d bytes to %s', len(contents), to_remote_path)
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
