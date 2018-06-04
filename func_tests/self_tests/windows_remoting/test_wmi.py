import logging

import pytest

from framework.os_access.windows_remoting import Users

log = logging.getLogger(__name__)


@pytest.fixture(scope='session')
def users(winrm):
    users = Users(winrm._protocol)
    return users


def test_user_profiles(users):
    assert users.all_profiles()
    assert users.system_profile()


def test_get_user(users):
    account = users.account_by_name(u'Administrator')
    assert account[u'Name'] == u'Administrator'
