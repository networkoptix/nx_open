from __future__ import division

import datetime
import errno
import logging
import math
import re
import sys
import timeit

import pytz
import six
import tzlocal.windows_tz

from framework.method_caching import cached_getter
from framework.networking.windows import WindowsNetworking
from framework.os_access import exceptions
from framework.os_access.command import DEFAULT_RUN_TIMEOUT_SEC
from framework.os_access.os_access_interface import OSAccess, Time, BaseFakeDisk
from framework.os_access.smb_path import SMBPath, UsedByAnotherProcess
from framework.os_access.windows_remoting import WinRM
from framework.os_access.windows_remoting.env_vars import EnvVars
from framework.os_access.windows_remoting.powershell import PowershellError
from framework.os_access.windows_remoting.users import Users
from framework.os_access.windows_traffic_capture import WindowsTrafficCapture
from framework.utils import RunningTime

_logger = logging.getLogger(__name__)


class WindowsTime(Time):
    def __init__(self, winrm):
        self.winrm = winrm

    def get_tz(self):
        timezone_result, = self.winrm.wmi.cls(u'Win32_TimeZone').enumerate({})  # Mind enumeration!
        windows_timezone_name = timezone_result[u'StandardName']
        # tzlocal.windows_tz.windows_tz contains popular timezones, including Pacific and Moscow time.
        # In case of problems, look for another timezone name mapping.
        # See: https://stackoverflow.com/a/16157307/1833960
        # See: https://stackoverflow.com/tags/timezone/info
        # See: https://unicode.org/cldr/charts/latest/supplemental/zone_tzid.html
        # See: https://data.iana.org/time-zones/tz-link.html
        if windows_timezone_name == u'Coordinated Universal Time':
            timezone = pytz.utc  # No such timezone in tzlocal.
        else:
            timezone_name = tzlocal.windows_tz.win_tz[windows_timezone_name]
            timezone = pytz.timezone(timezone_name)
        return timezone

    def get(self):
        started_at = timeit.default_timer()
        result = self.winrm.wmi.cls(u'Win32_OperatingSystem').singleton()
        delay_sec = timeit.default_timer() - started_at
        time = result[u'LocalDateTime'].astimezone(self.get_tz())
        return RunningTime(time, datetime.timedelta(seconds=delay_sec))

    def set(self, new_time):  # type: (datetime.datetime) -> RunningTime
        localized = new_time.astimezone(self.get_tz())
        started_at = timeit.default_timer()
        self.winrm.wmi.cls('Win32_OperatingSystem').static().invoke_method('SetDateTime', {'LocalDateTime': new_time})
        delay_sec = timeit.default_timer() - started_at
        return RunningTime(localized, datetime.timedelta(seconds=delay_sec))


class WindowsAccess(OSAccess):
    """Run CMD and PowerShell and access CIM/WMI via WinRM, access filesystem via SMB"""

    def __init__(self, host_alias, port_map, macs, username, password):
        self.winrm = WinRM(port_map.remote.address, port_map.remote.tcp(5985), username, password)
        path_cls = SMBPath.specific_cls(
            port_map.remote.address, port_map.remote.tcp(445),
            username, password)

        super(WindowsAccess, self).__init__(
            host_alias, port_map,
            WindowsNetworking(self.winrm, macs),
            WindowsTime(self.winrm),
            WindowsTrafficCapture(path_cls.tmp() / 'NetworkTrafficCapture', self.winrm),
            None,
            path_cls,
            )
        self._username = username

    def __repr__(self):
        return '<WindowsAccess via {!r}>'.format(self.winrm)

    @cached_getter
    def env_vars(self):
        """Effective environment: union of default, system's and user's vars"""
        users = Users(self.winrm)
        account = users.account_by_name(self._username)
        profile = users.profile_by_sid(account[u'SID'])
        profile_dir = profile[u'LocalPath']
        default_env_vars = {
            u'USERPROFILE': profile_dir,
            u'APPDATA': u'%USERPROFILE%\\Roaming',
            u'LOCALAPPDATA': u'%USERPROFILE%\\Local',
            u'PROGRAMFILES': u'C:\\Program Files',
            }
        env_vars = EnvVars.request(self.winrm, account[u'Caption'], default_env_vars)
        return env_vars

    @cached_getter
    def system_profile_dir(self):
        # TODO: Work via Users class.
        users = Users(self.winrm)
        system_profile = users.system_profile()
        profile_dir = system_profile[u'LocalPath']
        return profile_dir

    def run_command(self, command, input=None, logger=None, timeout_sec=DEFAULT_RUN_TIMEOUT_SEC):
        # TODO: add logger to winrm commands
        return self.winrm.run_command(command, input=input, timeout_sec=timeout_sec)

    def is_accessible(self):
        return self.winrm.is_working()

    def make_core_dump(self, pid):
        expected_exit_status = 0xFFFFFFFE  # ProcDump always exit with this.
        try:
            self.winrm.run_command(['procdump', '-accepteula', pid])  # Full dumps (`-ma`) are too big for pysmb.
        except exceptions.exit_status_error_cls(expected_exit_status):
            pass
        else:
            raise RuntimeError("Unexpected zero exit status, {} expected".format(expected_exit_status))

    def parse_core_dump(self, path, symbols_path='', timeout_sec=600):
        output = self.run_command(
            [
                # Its folder is not added to %PATH% by Chocolatey, but installation location is known (default).
                r'C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe',
                '-z', path,
                # `-y` is string with special format. Example: `cache*;srv*;\\server\share\symbols\product\v100500`.
                # See: https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/symbol-path
                '-y', symbols_path,
                '-c',  # Execute commands:
                '.exr -1;'  # last exception,
                '.ecxr;'  # last exception context,
                'kc;'  # current thread backtrace,
                '~*kc;'  # all threads backtrace,
                'q',  # quit.
                '-n',  # Verbose symbols loading.
                '-s',  # Disable deferred symbols load to make stack clear of loading errors and warnings.
                ],
            timeout_sec=timeout_sec,  # It's takes a while when symbols are not cached yet.
            )
        return output.decode('ascii')

    def fake_disk(self):
        return _FakeDisk(self)

    def _fs_root(self):
        return self.path_cls('C:\\')

    def _free_disk_space_bytes_on_all(self):
        mount_point_iter = self.winrm.wmi.cls('Win32_MountPoint').enumerate({})
        volume_iter = self.winrm.wmi.cls('Win32_Volume').enumerate({})
        volume_list = list(volume_iter)
        result = {}
        for mount_point in mount_point_iter:
            # Rely on fact that single identifying selector of `Directory` is its name.
            dir_path = self.path_cls(mount_point['Directory']['Name'])
            volume, = (v for v in volume_list if v.ref == mount_point['Volume'])
            result[dir_path] = int(volume['FreeSpace'])
        return result

    def _hold_disk_space(self, to_consume_bytes):
        holder_path = self._disk_space_holder()
        try:
            self._disk_space_holder().unlink()
        except exceptions.DoesNotExist:
            pass
        args = ['fsutil', 'file', 'createNew', holder_path, to_consume_bytes]
        self.winrm.command(args).run()

    def _download_by_http(self, source_url, destination_dir, timeout_sec):
        _, file_name = source_url.rsplit('/', 1)
        destination = destination_dir / file_name
        if destination.exists():
            return destination
        variables = {'out': str(destination), 'url': source_url, 'timeoutSec': timeout_sec}
        # language=PowerShell
        try:
            self.winrm.run_powershell_script('Invoke-WebRequest -OutFile $out $url -TimeoutSec $timeoutSec', variables)
        except PowershellError as e:
            raise exceptions.CannotDownload(str(e))
        return destination

    def _download_by_smb(self, source_hostname, source_path, destination_dir, timeout_sec):
        raise NotImplementedError(
            "Cannot download \\\\{!s}\\{!s}. ".format(source_hostname, source_path) +
            "Downloading from SMB share is not yet supported for Windows remote machines. "
            "There is second hop problem: connection to another machine via WinRM is not authorized. "
            "What has been attempted (but, probably, incorrectly, without full understanding): "
            "enabling CredSSP, specifying literal credentials in command itself. "
            "No classes has been found to establish connection via WMI (although it's possible to open them). "
            "What is to be attempted: Kerberos authentication. "
            "To debug try `Invoke-Command` from another Windows machine. "
            )

    def file_md5(self, path):
        try:
            output = self.winrm.command(['CertUtil', '-hashfile', path, 'md5']).run().decode()
        except exceptions.exit_status_error_cls(0x80070002):  # Misleading error code and message.
            raise exceptions.NotAFile(errno.EISDIR, "Trying to get MD5 of dir.")
        match = re.search(r'\b[0-9a-fA-F]{32}\b', output)
        if match is None:
            raise RuntimeError('Cannot get MD5 of {}:\n{}'.format(path, output))
        return match.group()


class _FakeDisk(BaseFakeDisk):
    def __init__(self, windows_access):  # type: (WindowsAccess) -> ...
        self._letter = 'V'
        super(_FakeDisk, self).__init__(windows_access.path_cls('{}:\\'.format(self._letter)))
        self._image_path = windows_access.path_cls('C:\\{}.vhdx'.format(self._letter))
        self.winrm = windows_access.winrm

    def _diskpart(self, name, script, timeout_sec):
        _logger.debug('Diskpart script:\n%s', script)
        script_path = self._image_path.with_name('{}.{}.diskpart.txt'.format(self._letter, name))
        script_path.write_text(script)
        # Error originate in VDS -- Virtual Disk Service.
        # See: https://msdn.microsoft.com/en-us/library/dd208031.aspx.
        command = self.winrm.command(['diskpart', '/s', script_path])
        try:
            command.run(timeout_sec=timeout_sec)
        except exceptions.exit_status_error_cls(0x80070057) as e:
            message_multiline = e.stdout.decode().rstrip().rsplit('\r\n\r\n')[-1]
            message_oneline = message_multiline.replace('\r\n', ' ')
            six.reraise(OSError, OSError(errno.EINVAL, message_oneline), sys.exc_info()[2])

    def remove(self, letter='V'):
        # `DETACH` fails even with `NOERR`. The following scheme helps to work around this and
        # save `diskpart` run if there is no disk mounted.
        try:
            _logger.debug("Try to delete virtual disk file first: %s", self._image_path)
            self._image_path.unlink()
        except exceptions.DoesNotExist:
            _logger.debug("Skip diskpart: no virtual disk file: %s", self._image_path)
        except UsedByAnotherProcess:
            _logger.debug("Run diskpart to dismount and detach: %s", self._image_path)
            script_template = (
                'SELECT VDISK file={image_path}' '\r\n'
                'DETACH VDISK' '\r\n'  # When detaching, volume is unmounted automatically.
            )
            script = script_template.format(image_path=self._image_path)
            self._diskpart('remove', script, 5)
            _logger.debug("Delete virtual disk file: %s", self._image_path)
            self._image_path.unlink()

    def mount(self, size_bytes):
        """Make virtual disk and mount it.
        When Windows 7 is not supported, move to Windows Storage Management Provider.
        See: https://docs.microsoft.com/en-us/previous-versions/windows/desktop/stormgmt/windows-storage-management-api-portal
        """
        free_space_mb = int(math.ceil(size_bytes / 1024 / 1024))
        volume_mb = free_space_mb + 15  # 15 MB is for System Volume Information.
        partition_mb = volume_mb + 1  # 1 MB for filesystem.
        disk_mb = partition_mb + 2  # 1 MB is for MBR/GPT headers.
        script_template = (
            'CREATE VDISK file={image_path} MAXIMUM={disk_mb}' '\r\n'  # NOERR if exists.
            'SELECT VDISK file={image_path}' '\r\n'  # No error if already selected.
            'ATTACH VDISK' '\r\n'  # "Disk" of "vdisk" has been selected.
            'CONVERT MBR' '\r\n'
            'CREATE PARTITION PRIMARY size={partition_mb}' '\r\n'  # New partition and its volume have been selected.
            'FORMAT' '\r\n'  # Filesystem is default (NTFS).
            'ASSIGN LETTER={letter}' '\r\n'
        )
        script = script_template.format(
            image_path=self._image_path,
            letter=self._letter,
            partition_mb=partition_mb,
            disk_mb=disk_mb)
        self._diskpart('mount', script, 10 + 0.05 * free_space_mb)

