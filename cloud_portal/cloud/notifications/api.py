from models import Message
from django.core.exceptions import ValidationError
import django
from notifications.config import notifications_config


def send(user_email, msg_type, message):
    user_email = django.core.validators.validate_email(user_email)
    msg = Message.objects.create(user_email=user_email, type=msg_type, message=message)

    # TODO: validate email among existing users

    if msg_type not in notifications_config:
        raise ValidationError(
            'Invalid message type',
            params={'value': msg_type})

    if not message:
        raise ValidationError(
            'Empty message',
            params={'value': message})

    msg.send()
