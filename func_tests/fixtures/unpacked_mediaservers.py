import logging
from argparse import ArgumentTypeError
from pprint import pformat

import pytest

from framework.installation.lightweight_mediaserver import LWS_BINARY_NAME
from framework.installation.unpacked_mediaserver_factory import UnpackedMediaserverFactory

_logger = logging.getLogger(__name__)


# we are expecting only one appserver2_ut in --mediaserver-installers-dir, for linux-x64 platform
@pytest.fixture()
def lightweight_mediaserver_installer(mediaserver_installers_dir):
    path = mediaserver_installers_dir / LWS_BINARY_NAME
    if not path.exists():
        raise ArgumentTypeError(
            '{} is missing from {}, but is required.'.format(LWS_BINARY_NAME, mediaserver_installers_dir))
    _logger.info("Ligheweight mediaserver installer path: {}".format(path))
    return path


@pytest.fixture()
def unpacked_mediaserver_factory(
        request, ca, artifacts_dir, mediaserver_installer_set, lightweight_mediaserver_installer):
    for installer in mediaserver_installer_set.installers:
        if installer.platform_variant == 'ubuntu' and installer.arch == 'x64':
            mediaserver_installer = installer
            break
    else:
        raise ValueError("Cannot find suitable installer among: {}".format(pformat(
            mediaserver_installer_set.installers)))
    return UnpackedMediaserverFactory(
        artifacts_dir, ca, mediaserver_installer, lightweight_mediaserver_installer,
        clean=request.config.getoption('--clean'))
