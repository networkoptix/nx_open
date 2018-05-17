import logging

from framework.dpkg_installation import install_mediaserver

_logger = logging.getLogger(__name__)


def test_install(linux_vm, mediaserver_deb):
    installation = install_mediaserver(linux_vm.os_access, mediaserver_deb, reinstall=True)
    assert installation.is_valid()
    installation = install_mediaserver(linux_vm.os_access, mediaserver_deb, reinstall=False)
    assert installation.is_valid()
