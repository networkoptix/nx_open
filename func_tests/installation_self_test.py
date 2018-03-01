import logging

import pytest

from test_utils.server_installation import install_mediaserver

_logger = logging.getLogger(__name__)


@pytest.mark.usefixtures('init_logging')
def test_install(vm_factory, bin_dir):
    vm = vm_factory.get()
    install_mediaserver(vm.guest_os_access, bin_dir / 'mediaserver.deb')
