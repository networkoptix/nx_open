import logging
from argparse import ArgumentTypeError

import pytest

from framework.installation.lightweight_mediaserver import LWS_BINARY_NAME
from framework.installation.make_installation import installer_by_vm_type
from framework.installation.unpacked_mediaserver_factory import UnpackedMediaserverFactory

_logger = logging.getLogger(__name__)


# we are expecting only one appserver2_ut in --mediaserver-installers-dir, for linux-x64 platform
@pytest.fixture(scope='session')
def lightweight_mediaserver_installer(mediaserver_installers_dir):
    path = mediaserver_installers_dir / LWS_BINARY_NAME
    if not path.exists():
        raise ArgumentTypeError(
            '{} is missing from {}, but is required.'.format(LWS_BINARY_NAME, mediaserver_installers_dir))
    _logger.info("Ligheweight mediaserver installer path: {}".format(path))
    return path


@pytest.fixture()
def unpacked_mediaserver_factory(
        request, ca, artifacts_dir, mediaserver_installers, lightweight_mediaserver_installer):
    mediaserver_installer = installer_by_vm_type(mediaserver_installers, vm_type='linux')
    return UnpackedMediaserverFactory(
        artifacts_dir, ca, mediaserver_installer, lightweight_mediaserver_installer,
        clean=request.config.getoption('--clean'))
