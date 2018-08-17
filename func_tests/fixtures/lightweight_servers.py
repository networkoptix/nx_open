import logging
from argparse import ArgumentTypeError

import pytest

from framework.installation.lightweight_mediaserver import LWS_BINARY_NAME

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
