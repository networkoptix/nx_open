import logging

import pytest

from framework.installation.make_installation import make_installation
from framework.installation.unpack_installation import UnpackedMediaserverGroup
from framework.installation.mediaserver_factory import setup_clean_mediaserver

_logger = logging.getLogger(__name__)


@pytest.fixture()
def installation(one_vm, mediaserver_installers):
    return make_installation(mediaserver_installers, one_vm.type, one_vm.os_access)


def test_install(installation):
    installation.install()
    assert installation.is_valid()


def test_group_install(linux_vm, mediaserver_installers, ca):
    group = UnpackedMediaserverGroup(linux_vm.os_access, mediaserver_installers['linux64'], linux_vm.os_access.Path('/tmp/srv'))
    installation, = group.make_installations(1701, 1)
    mediaserver = setup_clean_mediaserver('server', installation, ca)
    mediaserver.start()
    mediaserver.stop()
