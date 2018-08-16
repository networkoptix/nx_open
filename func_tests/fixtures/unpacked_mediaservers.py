import logging

import pytest

from framework.installation.make_installation import installer_by_vm_type
from framework.installation.unpacked_mediaserver_factory import UnpackedMediaserverFactory

pytest_plugins = ['fixtures.lightweight_servers']

_logger = logging.getLogger(__name__)


@pytest.fixture
def unpacked_mediaserver_factory(
        request, ca, artifacts_dir, mediaserver_installers, lightweight_mediaserver_installer):
    mediaserver_installer = installer_by_vm_type(mediaserver_installers, vm_type='linux')
    return UnpackedMediaserverFactory(
        artifacts_dir, ca, mediaserver_installer, lightweight_mediaserver_installer,
        clean=request.config.getoption('--clean'))
