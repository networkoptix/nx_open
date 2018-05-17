import logging
from pprint import pformat

import pytest

from framework.os_access.windows_remoting._services import WindowsService
from framework.os_access.windows_remoting._users import get_system_user_profile, get_user, all_user_profiles

log = logging.getLogger(__name__)


@pytest.mark.skip('Install mediaserver first')
@pytest.mark.parametrize('name', ['defaultMediaServer'])
def test_service(pywinrm_protocol, name):
    service = WindowsService(pywinrm_protocol, name)
    log.debug(pformat(service.is_running()))
    log.debug(pformat(service.stop()))
    log.debug(pformat(service.start()))


def test_user_profiles(pywinrm_protocol):
    log.debug(pformat(all_user_profiles(pywinrm_protocol)))
    log.debug(pformat(get_system_user_profile(pywinrm_protocol)))


def test_get_user(pywinrm_protocol):
    account, profile, vars = get_user(pywinrm_protocol, u'Administrator')
    assert account['Name'] == 'Administrator'
