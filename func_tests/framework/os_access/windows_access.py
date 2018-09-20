import datetime
import timeit

import dateutil.parser
import pytz
import tzlocal.windows_tz

from framework.method_caching import cached_getter
from framework.networking.windows import WindowsNetworking
from framework.os_access import exceptions
from framework.os_access.command import DEFAULT_RUN_TIMEOUT_SEC
from framework.os_access.os_access_interface import OSAccess, Time
from framework.os_access.smb_path import SMBPath
from framework.os_access.windows_remoting import WinRM
from framework.os_access.windows_remoting._powershell import PowershellError
from framework.os_access.windows_remoting.env_vars import EnvVars
from framework.os_access.windows_remoting.users import Users
from framework.os_access.windows_traffic_capture import WindowsTrafficCapture
from framework.utils import RunningTime


class WindowsTime(Time):
    def __init__(self, winrm):
        self.winrm = winrm

    @cached_getter
    def _get_timezone(self):
        timezone_result, = self.winrm.wmi_query(u'Win32_TimeZone',
                                                {}).enumerate()  # Mind enumeration!
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
        result = self.winrm.wmi_query(u'Win32_OperatingSystem', {}).get()
        delay_sec = timeit.default_timer() - started_at
        raw = result[u'LocalDateTime'][u'cim:Datetime']
        time = dateutil.parser.parse(raw).astimezone(self._get_timezone())
        return RunningTime(time, datetime.timedelta(seconds=delay_sec))

    def set(self, new_time):  # type: (datetime.datetime) -> RunningTime
        localized = new_time.astimezone(self._get_timezone())
        started_at = timeit.default_timer()
        # TODO: Do that with Win32_OperatingSystem.SetDateTime WMI method.
        # See: https://superuser.com/q/1323610/174311
        self.winrm.run_powershell_script(
            # language=PowerShell
            '''Set-Date $dateTime''',
            variables={'dateTime': new_time.astimezone(pytz.utc).isoformat()})
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

    def make_fake_disk(self, name, size_bytes):
        raise NotImplementedError()

    def free_disk_space_bytes(self):
        disks = self.winrm.wmi_query('Win32_LogicalDisk', {}).enumerate()
        disk_c, = (disk for disk in disks if disk['Name'] == 'C:')
        return int(disk_c['FreeSpace'])

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
            raise exceptions.AlreadyExists(
                "Cannot download {!s} to {!s}".format(source_url, destination_dir),
                destination)
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
