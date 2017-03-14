'''Customization support module

load information related to [current] customization
'''

import os.path
import ConfigParser


def read_customization_company_name(customization):
    proj_root = os.path.join(os.path.dirname(__file__), '../..')
    properties_path = os.path.join(proj_root, 'customization', customization, 'build.properties')
    config = ConfigParser.SafeConfigParser()
    with open(properties_path) as f:
        config.readfp(f)
    return config.get('basic', 'deb.customization.company.name')
