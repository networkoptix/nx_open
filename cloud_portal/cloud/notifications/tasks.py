from __future__ import absolute_import

from celery import shared_task
from celery.exceptions import Ignore

from .engines import email_engine

@shared_task
def send_email(user_email, type, message, customization, attempt=1):
    from smtplib import SMTPException
    from util.config import get_config

    error = ''
    try:
        email_engine.send(user_email, type, message, customization)
    except SMTPException as error:
        print(error)
        print("Attempt %s failed" % str(attempt))
        if attempt < get_config()['max_retries']:
            send_email.delay(user_email, type, message, customization, attempt+1)
            send_email.update_state(state="FAILURE", meta={'error': str(error),
                                                           'user_email': user_email,
                                                           'type': type,
                                                           'message': message,
                                                           'customization': customization,
                                                           'attempt': attempt})
        raise Ignore()
    else:
        return {'user_email': user_email, 'type': type, 'message': message,
                'customization': customization, 'attempt': attempt, 'error': str(error)}


@shared_task
def test_task(x, y):
    from time import sleep
    print("x: %i\ty:%i" % (x, y))
    sleep(y * 60)
    print("total: %i" % (x * y))
    with open('task.log', 'a+') as f:
        f.write("Task Done: %i * %i = %i" % (x, x, x*y))
    return x * y
