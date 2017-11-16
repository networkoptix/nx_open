import codecs
import pystache
from django.core.mail import EmailMultiAlternatives
# from email.mime.image import MIMEImage  # python 3
from email.MIMEImage import MIMEImage  # python 2
from django.conf import settings
import json
import os
from util.helpers import get_language_for_email
from cms.models import customization_cache, check_update_cache
from cms.controllers import filldata 
from django.core.cache import cache, caches


def email_cache(customization_name, cache_type, value=None, force=None):
    data = cache.get('email_cache')
    global_id = 0
    if not data:
        data = {customization_name: {'version_id': global_id}}

    if customization_name in data and 'version_id' in data[customization_name]:
        global_force, global_id = check_update_cache(customization_name, data[customization_name]['version_id'])
        if not force:
            force = global_force

    if data and customization_name in data and 'version_id' in data[customization_name]\
            and data[customization_name]['version_id'] != global_id:
        force = True

    if not data:
        data = {}

    if customization_name not in data or force:
        data[customization_name] = {'version_id': global_id}

    if cache_type not in data[customization_name]:
        data[customization_name][cache_type] = {}

    if value:
        data[customization_name][cache_type] = value
        cache.set('email_cache', data)
    else:
        cache.set('email_cache', data)
        return data[customization_name][cache_type]


def send(email, msg_type, message, customization_name):
    language_code = get_language_for_email(email, customization_name)

    config = {
        'portal_url': customization_cache(customization_name, "portal_url")
    }

    subject = get_email_title(customization_name, language_code, msg_type)
    subject = pystache.render(subject, {"message": message, "config": config})

    message_html_template = read_template(customization_name, msg_type, language_code, True)
    message_txt_template = read_template(customization_name, msg_type, language_code, False)

    email_html_body = pystache.render(message_html_template, {"message": message, "config": config})
    email_txt_body = pystache.render(message_txt_template, {"message": message, "config": config})

    email_from_name = customization_cache(customization_name, "mail_from_name")
    email_from_email = customization_cache(customization_name, "mail_from_email")
    email_from = '%s <%s>' % (email_from_name, email_from_email)

    msg = EmailMultiAlternatives(
        subject, email_txt_body, email_from, to=(email,))
    msg.content_subtype = 'plain'  # Main content is now text/html
    msg.attach_alternative(email_html_body, "text/html")

    # msg = EmailMultiAlternatives(subject, email_html_body, email_from, to=(email,))
    # msg.content_subtype = 'html'  # Main content is now text/html
    # msg.attach_alternative(email_txt_body, "text/plain")

    msg.mixed_subtype = 'related'
    msg_img = MIMEImage(read_file(customization_name, 'templates/email_logo.png'))
    msg_img.add_header('Content-ID', '<logo>')
    msg.attach(msg_img)
    return msg.send()


def get_email_title(customization_name, language_code, event):
    titles_cache = email_cache(customization_name, 'email_titles')
    if language_code not in titles_cache:
        filename = "templates/lang_{{language}}/notifications-language.json"
        data = read_file(customization_name, filename, language_code)
        titles_cache[language_code] = json.load(data)
        email_cache(customization_name, 'email_titles', titles_cache)
    return titles_cache[language_code][event]["emailSubject"]


def read_template(customization_name, name, language_code, html):
    suffix = ''
    if not html:
        suffix = '.txt'
    filename = os.path.join("templates/lang_{{language}}", name + suffix + '.mustache')
    return read_file(customization_name, filename, language_code)
    

def read_file(customization_name, filename, language_code=None):
    files_cache = email_cache(customization_name, 'files')
    translated_name = filename.replace("{{language}}", language_code)
    if translated_name not in files_cache:
        files_cache[translated_name] = filldata.read_customized_file(filename, customization_name, language_code)
        email_cache(customization_name, 'files', files_cache)
    return files_cache[translated_name]
