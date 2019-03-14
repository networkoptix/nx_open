import pystache
from django.core.mail import EmailMultiAlternatives
from django.core.mail.backends.smtp import EmailBackend
# from email.mime.image import MIMEImage  # python 3
from email.MIMEImage import MIMEImage  # python 2
import json
import os
from cms.models import cloud_portal_customization_cache, check_update_cache, get_cloud_portal_product
from cms.controllers import filldata 
from django.core.cache import cache


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
        return data[customization_name][cache_type]


def send(email, msg_type, message, language_code, customization_name):

    config = {
        'portal_url': cloud_portal_customization_cache(customization_name, "portal_url")
    }

    subject = get_email_title(customization_name, language_code, msg_type)
    subject = pystache.render(subject, {"message": message, "config": config})

    message_html_template = read_template(customization_name, msg_type, language_code, True)
    message_txt_template = read_template(customization_name, msg_type, language_code, False)

    email_html_body = pystache.render(message_html_template, {"message": message, "config": config})
    email_txt_body = pystache.render(message_txt_template, {"message": message, "config": config})

    email_from_name = cloud_portal_customization_cache(customization_name, "mail_from_name")
    email_from_email = cloud_portal_customization_cache(customization_name, "mail_from_email")
    email_from = '%s <%s>' % (email_from_name, email_from_email)

    mail_obj = EmailBackend(
        host=cloud_portal_customization_cache(customization_name, "smtp_host"),
        port=int(cloud_portal_customization_cache(customization_name, "smtp_port")),
        password=str(cloud_portal_customization_cache(customization_name, "smtp_password")),
        username=str(cloud_portal_customization_cache(customization_name, "smtp_user")),
        use_tls=cloud_portal_customization_cache(customization_name, "smtp_tls"),
    )

    msg = EmailMultiAlternatives(
        subject, email_txt_body, email_from, to=(email,))
    msg.content_subtype = 'plain'  # Main content is now text/html
    msg.attach_alternative(email_html_body, "text/html")

    # msg = EmailMultiAlternatives(subject, email_html_body, email_from, to=(email,))
    # msg.content_subtype = 'html'  # Main content is now text/html
    # msg.attach_alternative(email_txt_body, "text/plain")

    msg.mixed_subtype = 'related'
    msg_img = MIMEImage(read_file(customization_name, 'templates/email_logo.png'), _subtype="png")
    msg_img.add_header('Content-ID', '<logo>')
    msg.attach(msg_img)

    mail_obj.send_messages([msg])
    mail_obj.close()
    return True


def get_email_title(customization_name, language_code, event):
    titles_cache = email_cache(customization_name, 'email_titles')
    if language_code not in titles_cache:
        filename = "templates/lang_{{language}}/notifications-language.json"
        data = read_file(customization_name, filename, language_code)
        titles_cache[language_code] = json.loads(data)
        email_cache(customization_name, 'email_titles', titles_cache)
    return titles_cache[language_code][event]["emailSubject"]


def read_template(customization_name, name, language_code, html):
    suffix = ''
    if not html:
        suffix = '.txt'
    filename = os.path.join("templates/lang_{{language}}", name + suffix + '.mustache')
    return read_file(customization_name, filename, language_code)
    

def read_file(customization_name, filename, language_code=""):
    files_cache = email_cache(customization_name, 'files')
    translated_name = filename.replace("{{language}}", language_code)
    if translated_name not in files_cache:
        files_cache[translated_name] = filldata.read_customized_file(filename,
                                                                     get_cloud_portal_product(customization_name),
                                                                     language_code)
        email_cache(customization_name, 'files', files_cache)
    return files_cache[translated_name]
