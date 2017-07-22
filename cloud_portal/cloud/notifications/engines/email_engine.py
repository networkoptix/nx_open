import codecs
import pystache
from django.core.mail import EmailMultiAlternatives
# from email.mime.image import MIMEImage  # python 3
from email.MIMEImage import MIMEImage  # python 2
from django.conf import settings
import json, os
from util.config import get_config
from util.helpers import get_language_for_email


titles_cache = {}
templates_cache = {}
configs_cache = {}
logos_cache = {}


def send(email, msg_type, message, customization):
    custom_config = get_custom_config(customization)
    lang = get_language_for_email(email, custom_config['languages'])

    templates_root = os.path.join(settings.STATIC_LOCATION, customization, "templates")
    templates_location = os.path.join(templates_root, "lang_" + lang)
    subject = msg_type

    config = {
        'portal_url': custom_config['cloud_portal']['url']
    }

    subject = custom_config["mail_prefix"] + ' ' + get_email_title(customization, lang, msg_type, templates_location)
    subject = pystache.render(subject, {"message": message, "config": config})

    message_html_template = read_template(msg_type, templates_location, True)
    message_txt_template = read_template(msg_type, templates_location, False)

    email_html_body = pystache.render(message_html_template, {"message": message, "config": config})
    email_txt_body = pystache.render(message_txt_template, {"message": message, "config": config})
    email_from = custom_config["mail_from"]

    msg = EmailMultiAlternatives(subject, email_txt_body, email_from, to=(email,))
    msg.content_subtype = 'plain'  # Main content is now text/html
    msg.attach_alternative(email_html_body, "text/html")

    # msg = EmailMultiAlternatives(subject, email_html_body, email_from, to=(email,))
    # msg.content_subtype = 'html'  # Main content is now text/html
    # msg.attach_alternative(email_txt_body, "text/plain")

    msg.mixed_subtype = 'related'
    logo_filename = os.path.join(templates_root, 'email_logo.png')
    msg_img = MIMEImage(read_logo(logo_filename))
    msg_img.add_header('Content-ID', '<logo>')
    msg.attach(msg_img)
    return msg.send()


def get_custom_config(customization):
    if customization not in configs_cache:
        configs_cache[customization] = get_config(customization)
    return configs_cache[customization]


def get_email_title(customization, lang, event, templates_location):
    if customization not in titles_cache:
        titles_cache[customization] = {}
    if lang not in titles_cache[customization]:
        filename = os.path.join(templates_location, "notifications-language.json")
        with open(filename) as data_file:
            titles_cache[customization][lang] = json.load(data_file)
    return titles_cache[customization][lang][event]["emailSubject"]


def read_template(name, location, html):
    suffix = ''
    if not html:
        suffix = '.txt'
    filename = os.path.join(location, name + suffix + '.mustache')
    if filename not in templates_cache:
        try:
            # filename = pkg_resources.resource_filename('relnotes', 'templates/{0}.mustache'.format(name))
            with codecs.open(filename, 'r', 'utf-8') as stream:
                templates_cache[filename] = stream.read()
        except Exception as e:
            raise type(e)(e.message + ' :' + filename)
    return templates_cache[filename]


def read_logo(filename):
    if filename not in logos_cache:
        with open(filename, 'rb') as fp:
            logos_cache[filename] = fp.read()
    return logos_cache[filename]


def update_logo_cache(file_name, image):
    global logos_cache
    logos_cache[file_name] = image
