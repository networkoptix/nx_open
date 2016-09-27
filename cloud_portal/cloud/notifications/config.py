import os.path

from util.config import get_config
from os.path import join


conf = get_config()

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEMPLATES_LOCATION = join(BASE_DIR, "static", conf['customization'], "templates")

notifications_config = {
    'activate_account': {
        'engine': 'email',
        'subject': '[CLOUD] Confirm your account'
    },
    'restore_password': {
        'engine': 'email',
        'subject': '[CLOUD] Restore your password'
    },
    'system_invite': {
        'engine': 'email',
        'subject': '[CLOUD] Video system was shared with you'
    },
    'system_shared': {
        'engine': 'email',
        'subject': '[CLOUD] Video system was shared with you'
    },
}

notifications_module_config = {
    'portal_url': conf['cloud_portal']['url']
}
