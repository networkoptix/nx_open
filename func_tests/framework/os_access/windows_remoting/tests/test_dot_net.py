import logging
from pprint import pformat

import pytest

from framework.os_access.windows_remoting.dot_net.files import rename_file, get_file_info
from framework.os_access.windows_remoting.dot_net.services import Service
from framework.os_access.windows_remoting.dot_net.users import enumerate_user_profiles, get_system_user_profile

log = logging.getLogger(__name__)


@pytest.mark.parametrize('name', ['defaultMediaServer'])
def test_service(protocol, name):
    service = Service(protocol, name)
    log.debug(pformat(service.get()))
    log.debug(pformat(service.stop()))
    log.debug(pformat(service.start()))


@pytest.mark.parametrize('path', ['C:\\oi.txt'])
def test_file(protocol, path):
    path_moved_to = path + '.moved'
    log.debug(pformat(get_file_info(protocol, path)))
    rename_file(protocol, path, path_moved_to)
    rename_file(protocol, path_moved_to, path)


def test_user_profiles(protocol):
    log.debug(pformat(enumerate_user_profiles(protocol)))
    log.debug(pformat(get_system_user_profile(protocol)))
