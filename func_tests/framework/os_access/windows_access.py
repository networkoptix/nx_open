import datetime
import timeit

import dateutil.parser
import pytz
import tzlocal.windows_tz

from framework.method_caching import cached_getter, cached_property
from framework.networking.windows import WindowsNetworking
from framework.os_access.exceptions import exit_status_error_cls
from framework.os_access.os_access_interface import OSAccess
from framework.os_access.smb_path import SMBConnectionPool, SMBPath
from framework.os_access.windows_remoting import WinRM
from framework.os_access.windows_remoting.env_vars import EnvVars
from framework.os_access.windows_remoting.users import Users
from framework.utils import RunningTime


class WindowsAccess(OSAccess):
    """Run CMD and PowerShell and access CIM/WMI via WinRM, access filesystem via SMB"""

    def __init__(self, forwarded_ports, macs, username, password):
        self.macs = macs
        self._username = username
        self._password = password
        winrm_address, winrm_port = forwarded_ports['tcp', 5985]
        self.winrm = WinRM(winrm_address, winrm_port, username, password)
        self._forwarded_ports = forwarded_ports

    def __repr__(self):
        return '<WindowsAccess via {!r}>'.format(self.winrm)

    @property
    def forwarded_ports(self):
        return self._forwarded_ports

    @cached_getter
    def env_vars(self):
        """Effective environment: union of default, system's and user's vars"""
        users = Users(self.winrm)
        account = users.account_by_name(self._username)
        profile = users.profile_by_sid(account[u'SID'])
        profile_dir = profile[u'LocalPath']
        default_env_vars = {
            u'USERPROFILE': profile_dir,
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

    @cached_property
    def Path(self):
        class SpecificSMBPath(SMBPath):
            _direct_smb_address, _direct_smb_port = self.forwarded_ports['tcp', 445]
            _smb_connection_pool = SMBConnectionPool(
                self._username, self._password,
                _direct_smb_address, _direct_smb_port,
                )

            @classmethod
            def tmp(cls):
                env_vars = self.env_vars()
                temp_dir = cls(env_vars[u'TEMP'])
                return temp_dir / u'FuncTests'

            @classmethod
            def home(cls):
                env_vars = self.env_vars()
                return cls(env_vars[u'USERPROFILE'])

        return SpecificSMBPath

    def run_command(self, command, input=None):
        return self.winrm.run_command(command, input=input)

    def is_accessible(self):
        return self.winrm.is_working()

    @cached_property
    def networking(self):
        return WindowsNetworking(self.winrm, self.macs)

    @cached_getter
    def _get_timezone(self):
        timezone_result, = self.winrm.wmi_query(u'Win32_TimeZone', {}).enumerate()  # Mind enumeration!
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

    def get_time(self):
        started_at = timeit.default_timer()
        result = self.winrm.wmi_query(u'Win32_OperatingSystem', {}).get_one()
        delay_sec = timeit.default_timer() - started_at
        raw = result[u'LocalDateTime'][u'cim:Datetime']
        time = dateutil.parser.parse(raw).astimezone(self._get_timezone())
        return RunningTime(time, datetime.timedelta(seconds=delay_sec))

    def set_time(self, new_time):  # type: (datetime.datetime) -> RunningTime
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

    def make_core_dump(self, pid):
        expected_exit_status = 0xFFFFFFFE  # ProcDump always exit with this.
        try:
            self.winrm.run_command(['procdump', '-accepteula', pid])  # Full dumps (`-ma`) are too big for pysmb.
        except exit_status_error_cls(expected_exit_status):
            pass
        else:
            raise RuntimeError("Unexpected zero exit status, {} expected".format(expected_exit_status))
