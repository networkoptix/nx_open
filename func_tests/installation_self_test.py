import logging

import pytest

from test_utils.server_installation import install_media_server

_logger = logging.getLogger(__name__)


@pytest.mark.usefixtures('init_logging')
def test_install(box, bin_dir):
    box = box('server')
    install_media_server(box.guest_os_access, bin_dir / 'mediaserver.deb')
