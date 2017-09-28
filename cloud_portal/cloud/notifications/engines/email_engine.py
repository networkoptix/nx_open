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
from django.core.cache import cache


def email_cache(customization, cache_type, value=None, force=False):
    data = cache.get('email_cache')
    global_id = 0

    if not data:
        data = {customization: {'version_id': global_id}}

    if customization in data and 'version_id' in data[customization]:
        global_force, global_id = check_update_cache(customization, data[customization]['version_id'])
        if not force:
            force = global_force

    if customization not in data or force:
        data[customization] = {'version_id': global_id}

    if cache_type not in data[customization]:
        data[customization][cache_type] = {}

    if value:
        data[customization][cache_type] = value
        cache.set('email_cache', data)
    else:
        cache.set('email_cache', data)
        return data[customization][cache_type]


def send(email, msg_type, message, customization):
    lang = get_language_for_email(email, customization)

    templates_root = os.path.join(
        settings.STATIC_LOCATION, customization, "templates")
    templates_location = os.path.join(templates_root, "lang_" + lang)

    config = {
        'portal_url': customization_cache(customization, "portal_url")
    }

    subject = get_email_title(customization, lang, msg_type, templates_location)
    subject = pystache.render(subject, {"message": message, "config": config})

    message_html_template = read_template(customization, msg_type, templates_location, True)
    message_txt_template = read_template(customization, msg_type, templates_location, False)

    email_html_body = pystache.render(
        message_html_template, {"message": message, "config": config})
    email_txt_body = pystache.render(
        message_txt_template, {"message": message, "config": config})

    email_from_name = customization_cache(customization, "mail_from_name")
    email_from_email = customization_cache(customization, "mail_from_email")
    email_from = '%s <%s>' % (email_from_name, email_from_email)

    msg = EmailMultiAlternatives(
        subject, email_txt_body, email_from, to=(email,))
    msg.content_subtype = 'plain'  # Main content is now text/html
    msg.attach_alternative(email_html_body, "text/html")

    # msg = EmailMultiAlternatives(subject, email_html_body, email_from, to=(email,))
    # msg.content_subtype = 'html'  # Main content is now text/html
    # msg.attach_alternative(email_txt_body, "text/plain")

    msg.mixed_subtype = 'related'
    logo_filename = os.path.join(templates_root, 'email_logo.png')
    msg_img = MIMEImage(read_logo(customization, logo_filename))
    msg_img.add_header('Content-ID', '<logo>')
    msg.attach(msg_img)
    return msg.send()


def get_email_title(customization, lang, event, templates_location):
    titles_cache = email_cache(customization, 'email_titles')

    if lang not in titles_cache:
        filename = os.path.join(
            templates_location, "notifications-language.json")
        with open(filename) as data_file:
            titles_cache[lang] = json.load(data_file)
        email_cache(customization, 'email_titles', titles_cache)

    return titles_cache[lang][event]["emailSubject"]


def read_template(customization, name, location, html):
    templates_cache = email_cache(customization, 'email_templates')

    suffix = ''
    if not html:
        suffix = '.txt'
    filename = os.path.join(location, name + suffix + '.mustache')
    if filename not in templates_cache:
        try:
            # filename = pkg_resources.resource_filename('relnotes', 'templates/{0}.mustache'.format(name))
            with codecs.open(filename, 'r', 'utf-8') as stream:
                templates_cache[filename] = stream.read()
            email_cache(customization, 'email_templates', templates_cache)
        except Exception as e:
            raise type(e)(e.message + ' :' + filename)
    return templates_cache[filename]


def read_logo(customization, filename):
    logos_cache = email_cache(customization, 'logos')
    modified_file_time_cache = email_cache(customization, 'modified_file_times')

    # os.stat(filename)[8] gets the modified time for the file
    # If the file is not cached then it needs to be added
    # If the file is cached but the modified time is different from the time
    # that was saved then it needs to be updated
    if filename not in logos_cache or filename in modified_file_time_cache\
            and os.stat(filename)[8] != modified_file_time_cache[filename]:
        with open(filename, 'rb') as fp:
            logos_cache[filename] = fp.read()
        # After the file has been updated safe the modified time to the
        # modified_file_time_cache
        modified_file_time_cache[filename] = os.stat(filename)[8]

        email_cache(customization, 'logos', logos_cache)
        email_cache(customization, 'modified_file_times', modified_file_time_cache)
        
    return logos_cache[filename]
