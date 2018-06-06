import logging

import pytest

from framework.os_access.windows_remoting.registry import WindowsRegistry
from framework.os_access.windows_remoting.users import Users

_logger = logging.getLogger(__name__)


@pytest.fixture(scope='session')
def users(winrm):
    users = Users(winrm)
    return users


def test_user_profiles(users):
    assert users.all_profiles()
    assert users.system_profile()


def test_get_user(users):
    account = users.account_by_name(u'Administrator')
    assert account[u'Name'] == u'Administrator'


@pytest.fixture(scope='session')
def windows_registry(winrm):
    return WindowsRegistry(winrm)


def test_registry_empty_key(windows_registry):
    path = u'HKEY_LOCAL_MACHINE\\SOFTWARE'
    assert not windows_registry.key(path).list_values()


def test_registry_key(windows_registry):
    path = u'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\DateTime\\Servers'
    assert windows_registry.key(path).list_values()
