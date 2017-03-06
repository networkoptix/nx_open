from __future__ import absolute_import

from celery import shared_task

from .engines import email_engine


from smtplib import SMTPException
from util.config import get_config
from celery.exceptions import Ignore

import traceback
import logging
logger = logging.getLogger(__name__)

MAX_RETRIES = get_config()['max_retries']

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

    logger.error(error_formatted)


@shared_task
def send_email(user_email, type, message, customization, attempt=1):
    try:
        email_engine.send(user_email, type, message, customization)
    except Exception as error:
        if isinstance(error, SMTPException) and attempt < MAX_RETRIES:
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


@shared_task
def test_task(x, y):
    from time import sleep
    print("x: %i\ty:%i" % (x, y))
    sleep(y * 60)
    print("total: %i" % (x * y))
    with open('task.log', 'a+') as f:
        f.write("Task Done: %i * %i = %i" % (x, x, x*y))
    return x * y
