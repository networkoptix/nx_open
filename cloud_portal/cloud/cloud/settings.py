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
LOCAL_ENVIRONMENT = 'runserver' in sys.argv
conf = get_config()


CUSTOMIZATION = os.getenv('CUSTOMIZATION')
if not CUSTOMIZATION:
    CUSTOMIZATION = conf['customization']

assert ('trafficRelay' in conf), 'Ivan, please add traffic relay to config for this instance'
assert ('bucket' in conf), 'Ivan, please add s3 bucket to config for this instance'

TRAFFIC_RELAY_HOST = '{systemId}.' + conf['trafficRelay']['host']  # {systemId}.relay-bur.vmsproxy.hdw.mx
TRAFFIC_RELAY_PROTOCOL = 'https://'

# Quick-start development settings - unsuitable for production
# See https://docs.djangoproject.com/en/1.8/howto/deployment/checklist/

# SECURITY WARNING: keep the secret key used in production secret!
SECRET_KEY = '03-b9bxxpjxsga(qln0@3szw3+xnu%6ph_l*sz-xr_4^xxrj!_'

# SECURITY WARNING: don't run with debug turned on in production!
DEBUG = 'debug' in conf and conf['debug'] or LOCAL_ENVIRONMENT

ALLOWED_HOSTS = ['*']

SKINS = ['blue', 'green', 'orange']
DEFAULT_SKIN = 'blue'

# Application definition

INSTALLED_APPS = (
    'dal',
    'dal_select2',
    'admin_tools',
    'admin_tools.menu',
    'admin_tools.theming',
    'admin_tools.dashboard',

    'django.contrib.admin',
    'django.contrib.auth',
    'django.contrib.contenttypes',
    'django.contrib.sessions',
    'django.contrib.messages',
    'django.contrib.staticfiles',

    'storages',

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


BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # /app/app
STATIC_LOCATION = os.path.join(BASE_DIR, "static")  # this is used for email_engine to find templates

STATIC_ROOT = os.path.join(BASE_DIR, "static/common/static")
if LOCAL_ENVIRONMENT:
    STATIC_ROOT = os.path.join(BASE_DIR, "static/common")
    STATICFILES_DIRS = (
        os.path.join(STATIC_LOCATION, CUSTOMIZATION, "static"),
        os.path.join(STATIC_LOCATION, CUSTOMIZATION, "static/lang_en_US"),
    )

ADMIN_TOOLS_INDEX_DASHBOARD = 'cloud.dashboard.CustomIndexDashboard'
ADMIN_TOOLS_MENU = 'cms.menu.CustomMenu'
ADMIN_DASHBOARD = ('cms.models.ContentVersion',
                   'cms.models.Context',
                   'cms.models.ContextProxy',
                   'cms.models.ContextTemplate',
                   'cms.models.Customization',
                   'cms.models.DataRecord',
                   'cms.models.DataStructure',
                   'cms.models.ExternalFile',
                   'cms.models.ProductType',
                   'cms.models.UserGroupsToCustomizationPermissions',
                   'django_celery_results.*',
                   'notifications.models.*',
                   'rest_hooks.*',
                   'zapier.models.*'
                   )

TEMPLATES = [
    {
        'BACKEND': 'django.template.backends.django.DjangoTemplates',
        'DIRS': (
            STATIC_ROOT,
            os.path.join(STATIC_LOCATION, CUSTOMIZATION),  # get rid of app/app hardcode
            os.path.join(STATIC_LOCATION, CUSTOMIZATION, 'templates'),
            os.path.join(BASE_DIR, 'django_templates'),
        ),
        'OPTIONS': {
            'context_processors': [
                'django.template.context_processors.debug',
                'django.template.context_processors.request',
                'django.contrib.auth.context_processors.auth',
                'django.contrib.messages.context_processors.messages',
                'django.template.context_processors.media',
            ],
            'loaders': [
                'django.template.loaders.filesystem.Loader',
                'django.template.loaders.app_directories.Loader',
                'admin_tools.template_loaders.Loader',
            ]
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
            'charset': 'utf8mb4',
            'init_command': 'SET \
                character_set_server=utf8mb4,\
                collation_server = utf8mb4_unicode_ci'
        }
    }
}

CACHES = {
    'default': {
        'BACKEND': 'django.core.cache.backends.locmem.LocMemCache'
    },
    "global": {
        "BACKEND": "django_redis.cache.RedisCache",
        "LOCATION": "redis://redis:6379/1",
        "OPTIONS": {
            "CLIENT_CLASS": "django_redis.client.DefaultClient",
        }
    }
}


if LOCAL_ENVIRONMENT:
    conf["cloud_db"]["url"] = 'https://cloud-dev2.hdw.mx/cdb'
    CACHES["global"] = {
        'BACKEND': 'django.core.cache.backends.db.DatabaseCache',
        'LOCATION': 'portal_cache',
    }

# Internationalization
# https://docs.djangoproject.com/en/1.8/topics/i18n/

LANGUAGE_CODE = 'en-us'

TIME_ZONE = 'UTC'

USE_I18N = True

USE_L10N = True

USE_TZ = False


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
            'handlers': ['console']
        },
        'api.views.utils': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console']
        },
        'api.helpers.exceptions': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console']
        },
        'api.controllers.cloud_gateway': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console']
        },
        'notifications.tasks': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console']
        },
        'api.account_backend': {  # explicitly mention all modules with loogers
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console']
        },
        'api.controller.cloud_api': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console']
        },
        'api.views.account': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console']
        },
        'cms.controllers.filldata': {
            'level': 'DEBUG',
            'propagate': True,
            'handlers': ['console']
        }
    }
}

# Static files (CSS, JavaScript, Images) and s3 config
# https://docs.djangoproject.com/en/1.8/howto/static-files/

STATIC_URL = '/static/'
MEDIA_ROOT = BASE_DIR + '/static/integrations/'
MEDIA_URL = '/static/integrations/'
# #End of s3 config


# START s3 config
AWS_STORAGE_BUCKET_NAME = conf['bucket']

S3_DOMAIN = conf['s3_domain'] if 's3_domain' in conf else '%s.s3.amazonaws.com'
AWS_S3_CUSTOM_DOMAIN = S3_DOMAIN % AWS_STORAGE_BUCKET_NAME
# mysite is a default name for the admin site
INTEGRATION_FILE_STORAGE = 'mysite.storage_backends.MediaStorage'
# END s3

if LOCAL_ENVIRONMENT:
    AWS_STORAGE_BUCKET_NAME = 'cloud-portal'

REST_FRAMEWORK = {
    'DEFAULT_AUTHENTICATION_CLASSES': (
        'rest_framework.authentication.TokenAuthentication',
        'rest_framework.authentication.SessionAuthentication',
    ),
    'DEFAULT_PERMISSION_CLASSES': (
       'rest_framework.permissions.AllowAny',
    )
}
# Used for Zapier
HOOK_EVENTS = {
    'user.send_zap_request': None
}

# Celery settings section

# Configure AWS SQS
# Broker_url = 'sqs://{aws_access_key_id}:{aws_secret_access_key}@'
# BROKER_TRANSPORT_OPTIONS
#   queue_name_prefix allows you to name the queue for sqs
#   region allows you to specify the aws region


BROKER_URL = os.getenv('QUEUE_BROKER_URL')
BROKER_CONNECTION_MAX_RETRIES = 1
if not BROKER_URL:
    BROKER_URL = 'sqs://'

BROKER_TRANSPORT_OPTIONS = {
    'queue_name_prefix' : conf['queue_name'] + '-',
    'region' : 'us-east-1'
}

RESULT_PERSISTENT = True
CELERY_RESULT_BACKEND = 'django-db'
CELERY_SEND_EVENTS = False
CELERYD_PREFETCH_MULTIPLIER = 0  # Allows worker to consume as many messages it wants
BROKER_HEARTBEAT = 10  # Supposed to check connection with broker

# / End of Celery settings section

CLOUD_CONNECT = {
    'url': conf['cloud_db']['url'],
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


CORS_ORIGIN_ALLOW_ALL = DEBUG
CORS_ALLOW_CREDENTIALS = DEBUG

SESSION_COOKIE_SECURE = not LOCAL_ENVIRONMENT
CSRF_COOKIE_SECURE = not LOCAL_ENVIRONMENT

USE_ASYNC_QUEUE = True

ADMINS = conf['admins']

EMAIL_SUBJECT_PREFIX = ''
EMAIL_HOST = conf['smtp']['host']
EMAIL_HOST_USER = conf['smtp']['user']
EMAIL_HOST_PASSWORD = conf['smtp']['password']
EMAIL_PORT = conf['smtp']['port']
EMAIL_USE_TLS = conf['smtp']['tls']


LINKS_LIVE_TIMEOUT = 300  # Five minutes

PASSWORD_REQUIREMENTS = {
    'minLength': 8,
    'requiredRegex': re.compile("^[\x21-\x7E]|[\x21-\x7E][\x20-\x7E]*[\x21-\x7E]$"),
    'commonList': 'static/_source/blue/static/scripts/commonPasswordsList.json'
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
    'review_version': {
        'engine': 'email'
    },
    'cloud_notification':{
        'engine': 'email',
        'queue': 'broadcast-notifications'
    },
    'cloud_invite':{
        'engine': 'email'
    }
}

BROADCAST_NOTIFICATIONS_SUPERUSERS_ONLY = DEBUG
NOTIFICATIONS_AUTO_SUBSCRIBE = False


UPDATE_JSON = 'http://updates.hdwitness.com.s3.amazonaws.com/updates.json'
DOWNLOADS_JSON = 'http://updates.hdwitness.com.s3.amazonaws.com/{{customization}}/downloads.json'
DOWNLOADS_VERSION_JSON = 'http://updates.hdwitness.com.s3.amazonaws.com/{{customization}}/{{build}}/downloads.json'

MAX_RETRIES = conf['max_retries']
CLEAR_HISTORY_RECORDS_OLDER_THAN_X_DAYS = 30
CMS_MAX_FILE_SIZE = 9

SUPERUSER_DOMAIN = '@networkoptix.com'  # Only user from this domain can have superuser permissions

# Use if you want to check user level permissions only users with the can_csv_<model_label>
# will be able to download csv files.
DJANGO_EXPORTS_REQUIRE_PERM = False
# Use if you want to disable the global django admin action. This setting is set to True by default.
DJANGO_CSV_GLOBAL_EXPORTS_ENABLED = False
