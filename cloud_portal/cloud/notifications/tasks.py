from __future__ import absolute_import

from celery import shared_task

from .engines import email_engine


@shared_task
def send_email(user_email, type, message, customization):
    return email_engine.send(user_email, type, message, customization)

@shared_task
def test_task(x,y):
    from time import sleep
    print("x: %i\ty:%i" % (x, y))
    sleep(y * 60)
    print("total: %i" % (x * y))
    with open('task.log', 'a+') as f:
        f.write("Task Done: %i * %i = %i" %(x,x,x*y))
    return x * y