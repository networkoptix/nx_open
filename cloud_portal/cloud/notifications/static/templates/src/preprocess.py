import cssutils
import logging
import htmlmin
from inlinestyler.utils import inline_css
import yaml
import os
from os.path import join
import re
import inlinestyler
assert inlinestyler.VERSION == (0, 2, 1), "Setup correct version of inlinestyler! Try 'pip install inlinestyler==0.2.1' and change requirements.txt accordingly"

cssutils.log.setLevel(logging.ERROR)

container_text = open('container.mustache.template').read()
css_content = open('styles.css').read()


def get_config():
    conf_dir = os.getenv('CLOUD_PORTAL_CONF_DIR')
    if not conf_dir:
        raise RuntimeError('CLOUD_PORTAL_CONF_DIR environment variable is undefined')
    return yaml.safe_load(open(join(conf_dir, 'cloud_portal.yaml')))


def read_branding(content):
    conf = get_config()
    customization = conf['customization']
    branding_file = open(join('..', '..', '..', '..', '..', 'customizations', customization, 'front_end', 'styles', '_custom_palette.scss')).read()
    pattern = re.compile("(\$.+?)\s*:\s*(#[0-9a-sA-F]{3,6})\s*;")
    for match in re.finditer(pattern, branding_file):
        groups = match.groups()
        content = content.replace(groups[0], groups[1])
    return content


css_content = read_branding(css_content)

# Replace variables in css style, using _custom_palette.scss

container_text = container_text.replace('{{>style_css}}', css_content)

for file in os.listdir("."):
    if file.endswith(".mustache.body"):
        template_name = file.replace('.mustache.body', '')
        new_file = open('../' + template_name + '.mustache', 'w')
        text = container_text.replace('{{>body}}', open(template_name + '.mustache.body').read())
        print >> new_file, htmlmin.minify(inline_css(text), remove_comments=True, remove_empty_space=True, remove_optional_attribute_quotes=False).encode('utf-8', 'replace')

"""
for f in ('camera_input', 'camera_ip_conflict', 'camera_motion', 'mediaserver_conflict', 'generic_event', 'mediaserver_failure', 'mediaserver_started', 'storage_failure', 'license_issue'):

    """

