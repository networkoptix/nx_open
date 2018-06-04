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

    @cached_property
    def Path(self):
        winrm = self.winrm

        class SpecificSMBPath(SMBPath):
            _name_service_address, _name_service_port = self.forwarded_ports['udp', 137]
            _session_service_address, _session_service_port = self.forwarded_ports['tcp', 139]
            _smb_connection_pool = SMBConnectionPool(
                self._username, self._password,
                _name_service_address, _name_service_port,
                _session_service_address, _session_service_port,
                )

            @classmethod
            def tmp(cls):
                env_vars = winrm.user_env_vars()
                return cls(env_vars[u'TEMP']) / 'FuncTests'

            @classmethod
            def home(cls):
                env_vars = winrm.user_env_vars()
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
