# Logging initialization and configuration for command-line utilities

import logging
import logging.config

from pathlib2 import Path
import yaml


def init_logging(config_file):
    full_path = Path(__file__).parent / '../logging-config' / config_file
    config_text = full_path.read_text()
    config = yaml.load(config_text)
    logging.config.dictConfig(config)
    logging.getLogger().setLevel(logging.DEBUG)  # default is warning
    logging.info('Logging is initialized from "%s".', full_path)
