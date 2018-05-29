import logging

import pytest

from framework.installation.windows_service import WindowsService
from framework.os_access.windows_remoting import Users

log = logging.getLogger(__name__)


@pytest.fixture(scope='session', params=['Spooler'])
def service(request, winrm):
    service = WindowsService(winrm, request.param)
    return service


def test_service(service):
    if service.is_running():
        service.stop()
        service.start()
    else:
        service.start()
        service.stop()


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
