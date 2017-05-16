import cssutils
import logging
import htmlmin
from inlinestyler.utils import inline_css
import yaml
import os
import shutil
from os.path import join
import re
import inlinestyler
assert inlinestyler.VERSION == (0, 2, 1), "Setup correct version of inlinestyler! Try 'pip install inlinestyler==0.2.1' and change requirements.txt accordingly"

cssutils.log.setLevel(logging.ERROR)

container_text = open('container.txt.mustache.template').read()
container_html = open('container.mustache.template').read()
css_content = open('styles.css').read()


def read_branding(content):
    branding_file = open('_custom_palette.scss').read()
    pattern = re.compile("(\$.+?)\s*:\s*(#[0-9a-sA-F]{3,6})\s*;")
    for match in re.finditer(pattern, branding_file):
        groups = match.groups()
        content = content.replace(groups[0], groups[1])
    return content


css_content = read_branding(css_content)

# Replace variables in css style, using _custom_palette.scss

container_html = container_html.replace('{{>style_css}}', css_content)

for file in os.listdir("."):
    if file.endswith(".mustache.html"):
        template_name = file.replace('.mustache.html', '')
        new_file = open('../' + template_name + '.mustache', 'w')
        text = container_html.replace('{{>body}}', open(template_name + '.mustache.html').read())
        print >> new_file, htmlmin.minify(inline_css(text), remove_comments=True, remove_empty_space=True, remove_optional_attribute_quotes=False).encode('utf-8', 'replace')

    if file.endswith(".mustache.txt"):
        template_name = file.replace('.mustache.txt', '')
        new_file = open('../' + template_name + '.txt.mustache', 'w')
        text = container_text.replace('{{>body}}', open(template_name + '.mustache.txt').read())
        print >> new_file, text


shutil.copyfile("notifications-language.json", "../notifications-language.json")

"""
for f in ('camera_input', 'camera_ip_conflict', 'camera_motion', 'mediaserver_conflict', 'generic_event', 'mediaserver_failure', 'mediaserver_started', 'storage_failure', 'license_issue'):

    """

