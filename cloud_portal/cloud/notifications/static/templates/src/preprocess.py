import cssutils
import logging
from inlinestyler.utils import inline_css

import os

cssutils.log.setLevel(logging.ERROR)

containter_text = open('container.mustache.template').read()
containter_text = containter_text.replace('{{>style_css}}', open('styles.css').read())

for file in os.listdir("."):
    if file.endswith(".mustache.body"):
        template_name = file.replace('.mustache.body', '')
        new_file = open('../' + template_name + '.mustache', 'w')
        text = containter_text.replace('{{>body}}', open(template_name + '.mustache.body').read())
        print >> new_file, inline_css(text).encode('utf-8', 'replace')

"""
for f in ('camera_input', 'camera_ip_conflict', 'camera_motion', 'mediaserver_conflict', 'generic_event', 'mediaserver_failure', 'mediaserver_started', 'storage_failure', 'license_issue'):

    """

