import logging
import subprocess

from framework.os_access.args import sh_augment_script, sh_command_to_script
from framework.os_access.exceptions import Timeout, exit_status_error_cls
from framework.os_access.local_path import LocalPath
from framework.os_access.popen_communicate import communicate

_logger = logging.getLogger(__name__)


class LocalAccess(object):
    Path = LocalPath

    def __init__(self):
        self.name = 'localhost'
        self.hostname = 'localhost'

    def __repr__(self):
        return '<LocalAccess>'

    @staticmethod
    def _make_kwargs(cwd, env, has_input):
        kwargs = {
            'close_fds': True,
            'bufsize': 16 * 1024 * 1024,
            'stdout': subprocess.PIPE,
            'stderr': subprocess.PIPE,
            'stdin': subprocess.PIPE if has_input else None}
        if cwd is not None:
            kwargs['cwd'] = str(cwd)
        if env is not None:
            kwargs['env'] = {name: str(value) for name, value in env.items()}
        return kwargs

    @classmethod
    def _communicate(cls, process, input, timeout_sec):
        exit_status, stdout, stderr = communicate(process, input, timeout_sec)
        if exit_status is None:
            raise Timeout(timeout_sec)
        if exit_status != 0:
            raise exit_status_error_cls(exit_status)(stdout, stderr)
        return stdout

    @classmethod
    def run_command(cls, command, input=None, cwd=None, env=None, timeout_sec=60 * 60):
        kwargs = cls._make_kwargs(cwd, env, input is not None)
        command = [str(arg) for arg in command]
        _logger.debug('Run command:\n%s', sh_command_to_script(command))
        process = subprocess.Popen(command, shell=False, **kwargs)
        return cls._communicate(process, input, timeout_sec)

    @classmethod
    def run_sh_script(cls, script, input=None, cwd=None, env=None, timeout_sec=60 * 60):
        augmented_script_to_run = sh_augment_script(script, None, None)
        augmented_script_to_log = sh_augment_script(script, cwd, env)
        _logger.debug('Run:\n%s', augmented_script_to_log)
        kwargs = cls._make_kwargs(cwd, env, input is not None)
        process = subprocess.Popen(augmented_script_to_run, shell=True, **kwargs)
        return cls._communicate(process, input, timeout_sec)
