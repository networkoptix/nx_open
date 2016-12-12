import codecs
import pystache
from django.core.mail import EmailMultiAlternatives
# from email.mime.image import MIMEImage  # python 3
from email.MIMEImage import MIMEImage  # python 2
from django.conf import settings
import json, os
from util.config import get_config

templates_cache = {}
configs_cache = {}
logos_cache = {}


def send(email, msg_type, message, customization):
    templates_location = os.path.join(settings.STATIC_LOCATION, customization, "templates")

    custom_config = get_custom_config(customization)
    subject = msg_type

    config = {
        'portal_url': custom_config['cloud_portal']['url']
    }

    if msg_type in settings.NOTIFICATIONS_CONFIG:
        subject = custom_config["mail_prefix"] + ' ' + settings.NOTIFICATIONS_CONFIG[msg_type]['subject']
    else:
        message = {"type": msg_type,
                   "data": json.dumps(message,
                                      indent=4,
                                      separators=(',', ': '))
                   }
        msg_type = 'unknown'

    message_template = read_template(msg_type, templates_location)
    email_body = pystache.render(message_template, {"message": message, "config": config})
    email_from = custom_config["mail_from"]

    msg = EmailMultiAlternatives(subject, email_body, email_from, to=(email,))
    msg.attach_alternative(email_body, "text/html")
    msg.mixed_subtype = 'related'

    msg.content_subtype = 'html'  # Main content is now text/html

    logo_filename = os.path.join(templates_location, 'email_logo.png')
    msg_img = MIMEImage(read_logo(logo_filename))
    msg_img.add_header('Content-ID', '<logo>')
    msg.attach(msg_img)
    return msg.send()


def get_custom_config(customization):
    if customization not in configs_cache:
        configs_cache[customization] = get_config(customization)
    return configs_cache[customization]


def read_template(name, location):
    filename = os.path.join(location, name + '.mustache')
    if filename not in templates_cache:
        # filename = pkg_resources.resource_filename('relnotes', 'templates/{0}.mustache'.format(name))
        with codecs.open(filename, 'r', 'utf-8') as stream:
            templates_cache[filename] = stream.read()
    return templates_cache[filename]


def read_logo(filename):
    if location not in logos_cache:
        with open(filename, 'rb') as fp:
            logos_cache[filename] = fp.read()
    return logos_cache[filename]
