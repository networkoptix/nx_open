from .models import Message, Event
from django.core.exceptions import ValidationError
import django
from django.conf import settings
notifications_config = settings.NOTIFICATIONS_CONFIG


def send(user_email, msg_type, message, customization):

    django.core.validators.validate_email(user_email)

    msg = Message(user_email=user_email, type=msg_type, message=message, customization=customization)

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


def notify(event_type, object, data):
    event = Event(type=event_type, object=object, data=data)
    event.send()
