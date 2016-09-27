import os
import yaml

from os.path import join


def get_config():
    customization = os.getenv('CUSTOMIZATION')

    conf_dir = os.getenv('CLOUD_PORTAL_BASE_CONF_DIR')
    if not conf_dir:
        raise RuntimeError('CLOUD_PORTAL_BASE_CONF_DIR environment variable is undefined')

    return yaml.safe_load(open(join(conf_dir, customization, 'cloud_portal.yaml')))
