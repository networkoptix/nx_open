import datetime
import sys
import time

import pytest
import pytz
from pathlib2 import Path

from framework.os_access.exceptions import Timeout
from framework.os_access.posix_shell import local_shell


@pytest.fixture(scope='session')
def os_access(one_vm):
    return one_vm.os_access


def test_get_set_time(os_access):
    os_access.get_time()
    os_access.set_time(datetime.datetime.now(pytz.utc) - datetime.timedelta(days=100))


def test_timeout():
    script = 'import time; time.sleep(60)'
    with pytest.raises(Timeout):
        result = local_shell.run_command([sys.executable, '-c', script], timeout_sec=1)


def test_unclosed_stdout():
    sleep_time_sec = 60
    script_path = Path(__file__).with_name('test_os_access_script.py')
    start_time = time.time()
    local_shell.run_command([sys.executable, script_path, str(sleep_time_sec)])
    assert time.time() - start_time < sleep_time_sec
