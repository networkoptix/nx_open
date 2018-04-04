import abc
import datetime

import pytz

from framework.os_access.args import args_to_command
from framework.utils import RunningTime


class NonZeroExitStatus(Exception):
    def __init__(self, command, exit_status, stdout, stderr):
        self.exit_status = exit_status
        self.command = command
        self.stdout = stdout
        self.stderr = stderr

    def __str__(self):
        return "Command %s returned non-zero exit status %d" % (args_to_command(self.command), self.exit_status)


class Timeout(Exception):
    def __init__(self, command, timeout_sec):
        self.command = command
        self.timeout_sec = timeout_sec

    def __str__(self):
        return "Command %s was timed out (%s sec)" % (args_to_command(self.command), self.timeout_sec)


class FileNotFound(Exception):
    def __init__(self, path):
        super(FileNotFound, self).__init__("File {} not found".format(path))


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
