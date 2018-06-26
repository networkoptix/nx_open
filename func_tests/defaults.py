import getpass
import logging
import os
import socket

from pathlib2 import Path

try:
    # noinspection PyCompatibility
    from ConfigParser import SafeConfigParser as ConfigParser, NoSectionError
except ImportError:
    # noinspection PyCompatibility
    from configparser import ConfigParser, NoSectionError

_logger = logging.getLogger(__name__)


def _get_defaults():
    config_parser = ConfigParser()
    path = Path(__file__).with_name('pytest.ini')
    config_parser.read(str(path))
    sections = ['defaults']  # Always present.
    try:
        sections.append(os.environ['PYTEST_DEFAULTS_SECTION'])  # Should start with 'defaults.'.
    except KeyError:
        sections.append('defaults.' + socket.gethostname())
        sections.append('defaults.' + getpass.getuser())
    config = {}
    for section in sections:
        try:
            items = config_parser.items(section)
        except NoSectionError:
            continue
        config.update(items)  # Latter have higher priorities.
    return config


defaults = _get_defaults()
