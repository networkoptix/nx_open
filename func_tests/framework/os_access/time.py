import datetime

import pytz

from framework.os_access.ssh_access import SSHAccess
from framework.utils import RunningTime


def set_time(ssh_access, new_time):  # type: (SSHAccess, datetime.datetime) -> RunningTime
    started_at = datetime.datetime.now(pytz.utc)
    ssh_access.run_command(['date', '--set', new_time.isoformat()])
    return RunningTime(new_time, datetime.datetime.now(pytz.utc) - started_at)
