import logging

import pytest

from test_utils.server_installation import install_mediaserver

_logger = logging.getLogger(__name__)


@pytest.mark.usefixtures('init_logging')
def test_install(linux_vm_pool, bin_dir):
    vm = linux_vm_pool.get()
    install_mediaserver(vm.os_access, bin_dir / 'mediaserver.deb')
