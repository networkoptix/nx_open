import datetime

import pytest
import pytz


@pytest.fixture(scope='session')
def os_access(one_vm):
    return one_vm.os_access


def test_get_set_time(os_access):
    os_access.get_time()
    os_access.set_time(datetime.datetime.now(pytz.utc) - datetime.timedelta(days=100))
