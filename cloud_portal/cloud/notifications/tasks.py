from __future__ import absolute_import
from celery import shared_task
from .engines import email_engine

from smtplib import SMTPException, SMTPConnectError
from ssl import SSLError
from celery.exceptions import Ignore

from django.conf import settings

from api.models import Account

import traceback
import logging
logger = logging.getLogger(__name__)

def log_error(error, user_email, type, message, customization, attempt):
    error_formatted = '\n{}:{}\nTarget Email: {}\nType: {}\nMessage:{}\nCustomization: {}\nAttempt: {}\nCall Stack: {}'\
        .format(error.__class__.__name__,
                error,
                user_email,
                type,
                message,
                customization,
                attempt,
                traceback.format_exc())

    if isinstance(error, SMTPException):
        logger.warning(error_formatted)
    else:
        logger.error(error_formatted)


@shared_task
def send_email(user_email, type, message, customization, attempt=1):
    try:
        email_engine.send(user_email, type, message, customization)
    except Exception as error:
        if (isinstance(error, SMTPException) or isinstance(error, SSLError)) and attempt < settings.MAX_RETRIES:
            send_email.delay(user_email, type, message, customization, attempt+1)

        log_error(error, user_email, type, message, customization, attempt)

        send_email.update_state(state="FAILURE", meta={'error': str(error),
                                                       'user_email': user_email,
                                                       'type': type,
                                                       'message': message,
                                                       'customization': customization,
                                                       'attempt': attempt,
                                                       })
        raise Ignore()
    else:
        return {'user_email': user_email, 'type': type, 'message': message,
                'customization': customization, 'attempt': attempt}


# For testing we dont want to send emails to everyone so we need to set
# "BROADCAST_NOTIFICATIONS_SUPERUSERS_ONLY = true" in cloud.settings
@shared_task
def send_to_all_users(message, force=False):
    # if forced and not testing dont apply any filters to send to all users
    users = Account.objects.all()

    if not force:
        users = users.filter(subscribe=True)

    if settings.BROADCAST_NOTIFICATIONS_SUPERUSERS_ONLY:
        users = users.filter(is_superuser=True)

    for user in users:
        message['full_name'] = user.get_full_name()
        send_email.delay(user.email, 'cloud_notification', message, user.customization)

    return {'subject': message['subject'], 'force': force}


@shared_task
def test_task(x, y):
    from time import sleep
    print("x: %i\ty:%i" % (x, y))
    sleep(y * 60)
    print("total: %i" % (x * y))
    with open('task.log', 'a+') as f:
        f.write("Task Done: %i * %i = %i" % (x, x, x*y))
    return x * y
