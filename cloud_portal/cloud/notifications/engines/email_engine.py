import codecs
import pystache
from django.core.mail import EmailMultiAlternatives
# from email.mime.image import MIMEImage  # python 3
from email.MIMEImage import MIMEImage  # python 2
from django.conf import settings
import json

TEMPLATES_LOCATION = settings.NOTIFICATIONS_TEMPLATES_LOCATION
notifications_module_config = settings.NOTIFICATIONS_CONFIG_DATA
notifications_config = settings.NOTIFICATIONS_CONFIG


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
    email_body = pystache.render(message_template, {"message": message, "config": notifications_module_config})
    email_text = ''

    msg = EmailMultiAlternatives(subject, email_body, to=(email,))
    msg.attach_alternative(email_body, "text/html")
    msg.mixed_subtype = 'related'

    msg.content_subtype = "html"  # Main content is now text/html

    logo_filename = TEMPLATES_LOCATION + '/email_logo.png'
    fp = open(logo_filename, 'rb')
    msg_img = MIMEImage(fp.read())
    fp.close()
    msg_img.add_header('Content-ID', '<logo>')
    msg.attach(msg_img)
    return msg.send()


def read_template(name):
    filename = TEMPLATES_LOCATION + ('/{0}.mustache'.format(name))

    # filename = pkg_resources.resource_filename('relnotes', 'templates/{0}.mustache'.format(name))
    with codecs.open(filename, 'r', 'utf-8') as stream:
        return stream.read()
