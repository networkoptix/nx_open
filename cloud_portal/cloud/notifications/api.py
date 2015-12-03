from models import Message
from django.core.exceptions import ValidationError
import django
from notifications.config import notifications_config
from django.conf import settings

import logging
logger = logging.getLogger('django')


def send(user_email, msg_type, message):

    django.core.validators.validate_email(user_email)

    msg = Message.objects.create(user_email=user_email, type=msg_type, message=message)

    # TODO: validate email among existing users

    if msg_type not in notifications_config:
        if not settings.DEBUG:
            raise ValidationError(
                'Invalid message type',
                params={'value': msg_type})

    if not message:
        raise ValidationError(
            'Empty message',
            params={'value': message})

    msg.send()
