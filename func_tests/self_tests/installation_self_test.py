import logging

from test_utils.server_installation import install_mediaserver

_logger = logging.getLogger(__name__)


def test_install(single_vm, mediaserver_deb):
    install_mediaserver(single_vm.os_access, mediaserver_deb)
