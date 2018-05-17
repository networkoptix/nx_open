import logging
from pprint import pformat

import pytest

from framework.os_access.windows_remoting.dot_net.files import get_file_info, rename_file
from framework.os_access.windows_remoting.dot_net.services import Service
from framework.os_access.windows_remoting.dot_net.users import get_system_user_profile, get_user, user_profiles

log = logging.getLogger(__name__)


@pytest.mark.skip('Install mediaserver first')
@pytest.mark.parametrize('name', ['defaultMediaServer'])
def test_service(pywinrm_protocol, name):
    service = Service(pywinrm_protocol, name)
    log.debug(pformat(service.get()))
    log.debug(pformat(service.stop()))
    log.debug(pformat(service.start()))


@pytest.mark.skip('Put file first')
@pytest.mark.parametrize('path', ['C:\\oi.txt'])
def test_file(pywinrm_protocol, path):
    path_moved_to = path + '.moved'
    file_info = get_file_info(pywinrm_protocol, path)
    log.debug(pformat(file_info))
    rename_file(pywinrm_protocol, path, path_moved_to)
    rename_file(pywinrm_protocol, path_moved_to, path)


def test_user_profiles(pywinrm_protocol):
    log.debug(pformat(user_profiles(pywinrm_protocol)))
    log.debug(pformat(get_system_user_profile(pywinrm_protocol)))


def test_get_user(pywinrm_protocol):
    account, profile, vars = get_user(pywinrm_protocol, u'Administrator')
    assert account['Name'] == 'Administrator'
