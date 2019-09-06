from cms.models import get_cloud_portal_product, Product
from notifications.models import Message, Event, Feedback
from django.core.exceptions import ValidationError
import django
from django.conf import settings
notifications_config = settings.NOTIFICATIONS_CONFIG


def find_message(external_id):
    # trying to find message and expect None otherwise
    msg = Message.objects.filter(external_id=external_id).first()
    if not msg:
        return None
    return msg


def send(user_email, msg_type, message, customization, external_id=None):

    django.core.validators.validate_email(user_email)

    msg = Message(user_email=user_email, type=msg_type,
                  message=message, customization=customization,
                  external_id=external_id)

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


def send_feedback(event_type, product_id, data):
    if not product_id:
        product = get_cloud_portal_product()
    else:
        product = Product.objects.get(id=product_id)

    feedback = Feedback.objects.create(sender_to_be_contacted=data['contact'],
                                       message=data['message'],
                                       product_name=data['product'],
                                       sender_name=data['sender_name'],
                                       sender_email=data['sender_email'],
                                       target_product=product,
                                       type=event_type)
    feedback.send()
