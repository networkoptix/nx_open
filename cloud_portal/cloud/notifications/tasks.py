from __future__ import absolute_import

from celery import shared_task

from .engines import email_engine

@shared_task
def send_email(user_email, type, message):
    return email_engine.send(user_email, type, message)
