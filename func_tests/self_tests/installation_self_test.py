import logging

import pytest

from framework.installation.make_installation import make_installation

_logger = logging.getLogger(__name__)


@pytest.fixture()
def installation(one_vm, mediaserver_packages):
    return make_installation(mediaserver_packages, one_vm.type, one_vm.os_access)


def test_install(installation):
    installation.install()
    assert installation.is_valid()
