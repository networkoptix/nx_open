import cssutils
import logging
import htmlmin
from inlinestyler.utils import inline_css

import os

cssutils.log.setLevel(logging.ERROR)

container_text = open('container.mustache.template').read()
container_text = container_text.replace('{{>style_css}}', open('styles.css').read())

for file in os.listdir("."):
    if file.endswith(".mustache.body"):
        template_name = file.replace('.mustache.body', '')
        new_file = open('../' + template_name + '.mustache', 'w')
        text = container_text.replace('{{>body}}', open(template_name + '.mustache.body').read())
        print >> new_file, htmlmin.minify(inline_css(text), remove_comments=True, remove_empty_space=True, remove_optional_attribute_quotes=False).encode('utf-8', 'replace')

"""
for f in ('camera_input', 'camera_ip_conflict', 'camera_motion', 'mediaserver_conflict', 'generic_event', 'mediaserver_failure', 'mediaserver_started', 'storage_failure', 'license_issue'):

    """

