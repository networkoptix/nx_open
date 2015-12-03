from notifications.helpers.emailSend import sendEmail
import codecs
import pystache
from django.core.mail import EmailMessage
from notifications.config import notifications_config
import json


def send(email, msg_type, message):
    # 1. get

    subject = msg_type
    if msg_type in notifications_config:
        subject = notifications_config[msg_type]['subject']
    else:
        message = {"type": msg_type,
                   "data": json.dumps(message,
                                      indent=4,
                                      separators=(',', ': '))
                   }
        msg_type = 'unknown'

    message_template = read_template(msg_type)
    message_body = pystache.render(message_template, {"message": message})

    email_template = read_template('email_body')
    email_body = pystache.render(email_template, {"message_body": message_body})

    msg = EmailMessage(subject, email_body, to=(email,))
    msg.content_subtype = "html"  # Main content is now text/html
    return msg.send()


def read_template(name):
    filename = 'notifications/static/templates/{0}.mustache'.format(name)

    # filename = pkg_resources.resource_filename('relnotes', 'templates/{0}.mustache'.format(name))
    with codecs.open(filename, 'r', 'utf-8') as stream:
        return stream.read()
