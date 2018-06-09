from abc import ABCMeta, abstractproperty

from framework.os_access.os_access_interface import OSAccess
from framework.os_access.posix_shell import PosixShell


class PosixAccess(OSAccess):
    __metaclass__ = ABCMeta

    @abstractproperty
    def shell(self):
        return PosixShell()

    def run_command(self, command, input=None):
        return self.shell.run_command(command, input=input)

    def make_core_dump(self, pid):
        self.shell.run_sh_script('gcore -o /proc/$PID/cwd/core.$(date +%s) $PID', env={'PID': pid})
