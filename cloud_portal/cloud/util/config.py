import os
import yaml
import os


def get_config(customization=None):
    if not customization:
        customization = os.getenv('CUSTOMIZATION')
    if not customization:
        customization = 'default'

    conf_dir = os.getenv('CLOUD_PORTAL_BASE_CONF_DIR')
    if not conf_dir:
        conf_dir = os.path.dirname(__file__)
    file_path = os.path.join(conf_dir, customization, 'cloud_portal.yaml')  # normal case - working instance
    if not os.path.isfile(file_path):  # this is for local environment
        file_path = os.path.join(conf_dir, '../../etc', 'cloud_portal.yaml')
    if not os.path.isfile(file_path):  # this is for Jenkins to collect static
        file_path = os.path.join(conf_dir, '..', 'cloud_portal.yaml')

    return yaml.safe_load(open(file_path))

