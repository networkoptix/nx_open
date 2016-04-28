from util.config import get_config

conf = get_config()

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
