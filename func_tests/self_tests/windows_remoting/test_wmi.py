import logging

import pytest

from framework.os_access.windows_remoting.users import Users

_logger = logging.getLogger(__name__)


@pytest.fixture()
def users(winrm):
    users = Users(winrm)
    return users


def test_user_profiles(users):
    assert users.all_profiles()
    assert users.system_profile()


def test_get_user(users):
    account = users.account_by_name(u'Administrator')
    assert account[u'Name'] == u'Administrator'
