from __future__ import absolute_import
from celery import shared_task
from .engines import email_engine

from smtplib import SMTPException, SMTPConnectError, SMTPServerDisconnected
from ssl import SSLError
from celery.exceptions import Ignore

from django.conf import settings

from api.models import Account
from notifications import notifications_api
from util.helpers import get_language_for_email

import traceback
import logging
logger = logging.getLogger(__name__)


class MaxResendException(Exception):
    def __str__(self):
        return "Emails was not sent because it hit max retry limit!!!"


def log_error(error, user_email, type, message, lang, customization, queue, attempt):
    error_formatted = '\n{}:{}\nTarget Email: {}\nType: {}\nMessage:{}\nLanguage: {}\nCustomization: {}\nQueue: {}\nAttempt: {}\nCall Stack: {}'\
        .format(error.__class__.__name__,
                error,
                user_email,
                type,
                message,
                lang,
                customization,
                queue,
                attempt,
                traceback.format_exc().replace("Traceback", ""))

    if isinstance(error, SMTPException) or isinstance(error, SMTPServerDisconnected):
        logger.warning(error_formatted)
    else:
        logger.error(error_formatted)


def send_email_log(_task):
    def wrapper(*args, **kwargs):
        logger.info("Start {} was run with args {}, kwargs: {}".format(_task.__name__, args, kwargs))
        return _task(*args, **kwargs)
    return wrapper


@shared_task
@send_email_log
def send_email(user_email, type, message, customization, queue="", attempt=1):
    lang = get_language_for_email(user_email, customization)
    try:
        email_engine.send(user_email, type, message, lang, customization)
    except Exception as error:
        if (isinstance(error, SMTPException) or isinstance(error, SSLError)) and attempt < settings.MAX_RETRIES:
            send_email.apply_async(args=[user_email, type, message, customization, queue, attempt+1],
                                   queue=queue)
        elif attempt >= settings.MAX_RETRIES:
            error = MaxResendException()

        log_error(error, user_email, type, message, lang, customization, queue, attempt)

        send_email.update_state(state="FAILURE", meta={'error': str(error),
                                                       'user_email': user_email,
                                                       'type': type,
                                                       'message': message,
                                                       'customization': customization,
                                                       'language': lang,
                                                       'queue': queue,
                                                       'attempt': attempt,
                                                       })
        raise Ignore()
    else:
        return {'user_email': user_email, 'type': type, 'message': message, 'customization': customization,
                'language': lang, 'queue': queue, 'attempt': attempt}


# For testing we dont want to send emails to everyone so we need to set
# "BROADCAST_NOTIFICATIONS_SUPERUSERS_ONLY = true" in cloud.settings
@shared_task
def send_to_all_users(notification_id, message, customizations, force=False):
    # if forced and not testing dont apply any filters to send to all users
    users = Account.objects.exclude(activated_date=None, last_login=None).filter(customization__in=customizations)
    if not force:
        users = users.filter(subscribe=True)

    if settings.BROADCAST_NOTIFICATIONS_SUPERUSERS_ONLY:
        users = users.filter(is_superuser=True)

    for user in users:
        message['userFullName'] = user.get_full_name()
        notifications_api.send(user.email, 'cloud_notification', message, user.customization)

    return {'notification_id': notification_id, 'subject': message['subject'], 'force': force}


@shared_task
def test_task(x, y):
    from time import sleep
    print("x: %i\ty:%i" % (x, y))
    sleep(y * 60)
    print("total: %i" % (x * y))
    with open('task.log', 'a+') as f:
        f.write("Task Done: %i * %i = %i" % (x, x, x*y))
    return x * y
