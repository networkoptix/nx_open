"""
Django settings for cloud project.

Generated by 'django-admin startproject' using Django 1.8.5.

For more information on this file, see
https://docs.djangoproject.com/en/1.8/topics/settings/

For the full list of settings and their values, see
https://docs.djangoproject.com/en/1.8/ref/settings/
"""

# Build paths inside the project like this: os.path.join(BASE_DIR, ...)
import os
import re
import json
import sys
from util.config import get_config

reload(sys)
sys.setdefaultencoding("utf-8")

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

conf = get_config()
CUSTOMIZATION = conf['customization']

# Quick-start development settings - unsuitable for production
# See https://docs.djangoproject.com/en/1.8/howto/deployment/checklist/

# SECURITY WARNING: keep the secret key used in production secret!
SECRET_KEY = '03-b9bxxpjxsga(qln0@3szw3+xnu%6ph_l*sz-xr_4^xxrj!_'

# SECURITY WARNING: don't run with debug turned on in production!
DEBUG = True

ALLOWED_HOSTS = ['*']


# Application definition

INSTALLED_APPS = (
    'django.contrib.admin',
    'django.contrib.auth',
    'django.contrib.contenttypes',
    'django.contrib.sessions',
    'django.contrib.messages',
    'django.contrib.staticfiles',
    'django_celery_results',
    'rest_framework',
    'rest_hooks',
    'corsheaders',
    'notifications',
    'api',
    'cms',
    'zapier',
    'tinymce'
)

MIDDLEWARE_CLASSES = (
    'django.contrib.sessions.middleware.SessionMiddleware',
    'corsheaders.middleware.CorsMiddleware',
    'django.middleware.common.CommonMiddleware',
    'django.middleware.csrf.CsrfViewMiddleware',
    'django.contrib.auth.middleware.AuthenticationMiddleware',
    'django.contrib.auth.middleware.SessionAuthenticationMiddleware',
    'django.contrib.messages.middleware.MessageMiddleware',
    'django.middleware.clickjacking.XFrameOptionsMiddleware',
    'django.middleware.security.SecurityMiddleware',
)

ROOT_URLCONF = 'cloud.urls'

TEMPLATES = [
    {
        'BACKEND': 'django.template.backends.django.DjangoTemplates',
        'DIRS': (
            '/app/app/static/{}'.format(CUSTOMIZATION), # Looks like static files used as templates
            '/app/app/static/{}/templates'.format(CUSTOMIZATION),
            '/app/app/templates'
        ),
        'APP_DIRS': True,
        'OPTIONS': {
            'context_processors': [
                'django.template.context_processors.debug',
                'django.template.context_processors.request',
                'django.contrib.auth.context_processors.auth',
                'django.contrib.messages.context_processors.messages',
                'django.template.context_processors.media',
            ],
        },
    },
]

WSGI_APPLICATION = 'cloud.wsgi.application'


# Database
# https://docs.djangoproject.com/en/1.8/ref/settings/#databases

cloud_db = conf['cloud_database']

DATABASES = {
    'default': {
        'ENGINE': 'django.db.backends.mysql',
        'HOST': cloud_db['host'],
        'PORT': cloud_db['port'],
        'USER': cloud_db['username'],
        'PASSWORD': cloud_db['password'],
        'NAME': cloud_db['database'],
        'OPTIONS': {
            'sql_mode': 'TRADITIONAL',
            'charset': 'utf8',
            'init_command': 'SET '
                'character_set_connection=utf8,'
                'collation_connection=utf8_bin'
        }
    }
}

CACHES = {
    'default': {
        'BACKEND': 'django.core.cache.backends.db.DatabaseCache',
        'LOCATION': 'portal_cache',
    }
}

# Internationalization
# https://docs.djangoproject.com/en/1.8/topics/i18n/

LANGUAGE_CODE = 'en-us'

TIME_ZONE = 'UTC'

USE_I18N = True

USE_L10N = True

USE_TZ = True


LOGGING = {
    'version': 1,
    'disable_existing_loggers': False,
    'formatters': {
        'verbose': {
            'format': '[%(levelname)s] %(asctime)s %(module)s %(process)d %(thread)d %(message)s'
        },
        'simple': {
            'format': '[%(levelname)s] %(message)s'
        },
    },
    'handlers': {
        'console': {
            'level': 'DEBUG',
            'class': 'logging.StreamHandler',
            'formatter': 'verbose'
        },
        'mail_admins': {
            'level': 'CRITICAL',
            'class': 'cloud.logger.LimitAdminEmailHandler',
            'formatter': 'simple'
        },
    },
    'loggers': {
        '': {  # default settings for all django loggers
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console', 'mail_admins']
        },
        'api.views.utils': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console', 'mail_admins']
        },
        'api.helpers.exceptions': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console', 'mail_admins']
        },
        'api.controllers.cloud_gateway': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console', 'mail_admins']
        },
        'notifications.tasks': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console', 'mail_admins']
        },
        'api.account_backend': {  # explicitly mention all modules with loogers
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console', 'mail_admins']
        },
        'api.controller.cloud_api': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console', 'mail_admins']
        },
        'api.views.account': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console', 'mail_admins']
        }
    }
}

# Static files (CSS, JavaScript, Images)
# https://docs.djangoproject.com/en/1.8/howto/static-files/

STATIC_URL = '/static/'

REST_FRAMEWORK = {
    'DEFAULT_AUTHENTICATION_CLASSES': (
        'rest_framework.authentication.TokenAuthentication',
        'rest_framework.authentication.SessionAuthentication',
    ),
    'DEFAULT_PERMISSION_CLASSES': (
       'rest_framework.permissions.AllowAny',
    )
}
HOOK_EVENTS = {
    'user.send_zap_request': None
}

# Celery settings section

#Configure AWS SQS
#Broker_url = 'sqs://{aws_access_key_id}:{aws_secret_access_key}@'
#BROKER_TRANSPORT_OPTIONS
#   queue_name_prefix allows you to name the queue for sqs
#   region allows you to specify the aws region



BROKER_URL = os.getenv('QUEUE_BROKER_URL')
BROKER_CONNECTION_MAX_RETRIES = 1
if not BROKER_URL:
    BROKER_URL = 'sqs://AKIAIQVGGMML4WNBECRA:jmXYHNKOAL9gYYaxAVClgegzShjaPF27ycvBOV1s@'
    BROKER_TRANSPORT_OPTIONS = {
        'queue_name_prefix' : conf['queue_name'] + '-',
        'region' : 'us-east-1'
    }

RESULT_PERSISTENT = True
CELERY_RESULT_BACKEND = 'django-db'
CELERY_SEND_EVENTS = False
WORKER_PREFETCH_MULTIPLIER = 1

# / End of Celery settings section

CLOUD_CONNECT = {
    'url': conf['cloud_db']['url'],
    'gateway': conf['cloud_gateway']['url'],
    # 'url': 'http://localhost:3346',
    # 'url': 'http://10.0.3.41:3346',
    'customization': CUSTOMIZATION,
    'password_realm': 'VMS'
}

# whitelist for unauthorized IP addresses. Supports addresses with masks
# read more: https://docs.python.org/3/library/ipaddress.html
IP_WHITELISTS = {
    'local': (
        '127.0.0.1',        # localhost
        '74.7.76.98',       # LA office
        '95.31.136.2',      # Moscow office
        '172.17.0.0/16',    # Docker
        '192.168.0.0/16',   # local network
        '10.0.0.0/16'       # Aws VPC
        )
}

AUTH_USER_MODEL = 'api.Account'
AUTHENTICATION_BACKENDS = ('api.account_backend.AccountBackend', )


CORS_ORIGIN_ALLOW_ALL = True  # TODO: Change this value on production!
CORS_ALLOW_CREDENTIALS = True


USE_ASYNC_QUEUE = True

ADMINS = conf['admins']

DEFAULT_FROM_EMAIL = conf['mail_from']
EMAIL_SUBJECT_PREFIX = conf['mail_prefix']
EMAIL_HOST = conf['smtp']['host']
EMAIL_HOST_USER = conf['smtp']['user']
EMAIL_HOST_PASSWORD = conf['smtp']['password']
EMAIL_PORT = conf['smtp']['port']
EMAIL_USE_TLS = conf['smtp']['tls']


SERVER_EMAIL = DEFAULT_FROM_EMAIL
LINKS_LIVE_TIMEOUT = 300  # Five minutes

PASSWORD_REQUIREMENTS = {
    'minLength': 8,
    'requiredRegex': re.compile("^[\x21-\x7E]|[\x21-\x7E][\x20-\x7E]*[\x21-\x7E]$"),
    'commonList': 'static/{}/static/scripts/commonPasswordsList.json'.format(CUSTOMIZATION)
}

common_list_file = PASSWORD_REQUIREMENTS['commonList']
if os.path.isfile(common_list_file):
    with open(common_list_file) as data_file:
        PASSWORD_REQUIREMENTS['common_passwords'] = json.load(data_file)
else:
    print >> sys.stderr, "Warning: Can't read from {}".format(common_list_file)


NOTIFICATIONS_CONFIG = {
    'activate_account': {
        'engine': 'email'
    },
    'restore_password': {
        'engine': 'email'
    },
    'system_invite': {
        'engine': 'email'
    },
    'system_shared': {
        'engine': 'email'
    },
}

NOTIFICATIONS_AUTO_SUBSCRIBE = False

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATIC_LOCATION = os.path.join(BASE_DIR, "static")
STATIC_ROOT = '/app/app/static/common/static'


LANGUAGES = conf['languages']
DEFAULT_LANGUAGE = conf['languages'][0]
UPDATE_JSON = 'http://updates.networkoptix.com/updates.json'

MAX_RETRIES = conf['max_retries']

SUPERUSER_DOMAIN = '@networkoptix.com'  # Only user from this domain can have superuser permissions

# Use if you want to check user level permissions only users with the can_csv_<model_label>
# will be able to download csv files.
DJANGO_EXPORTS_REQUIRE_PERM = False
# Use if you want to disable the global django admin action. This setting is set to True by default.
DJANGO_CSV_GLOBAL_EXPORTS_ENABLED = False