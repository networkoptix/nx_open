import cssutils
import logging
import htmlmin
from inlinestyler.utils import inline_css
import os
import shutil
import re
import inlinestyler
assert inlinestyler.VERSION == (0, 2, 1), "Setup correct version of inlinestyler! Try \"pip install inlinestyler==0.2.1\" and change requirements.txt accordingly"

cssutils.log.setLevel(logging.ERROR)

container_text = open("container.txt.mustache.template").read()
container_html = open("container.mustache.template").read()
css_content = open("styles.css").read()


def read_branding(content):
    branding_file = open("_custom_palette.scss").read()
    pattern = re.compile("(\$.+?)\s*:\s*(#[0-9a-sA-F]{3,6})\s*;")
    for match in re.finditer(pattern, branding_file):
        groups = match.groups()
        content = content.replace(groups[0], groups[1])
    return content


def write_html(file_name):
    template_name = file_name.replace(".mustache.html", "")
    with open("../" + template_name + ".mustache", "w") as new_file:
        text = container_html.replace("{{>body}}", open(template_name + ".mustache.html").read())
        print(htmlmin.minify(inline_css(text), remove_comments=True, remove_empty_space=True,
                             remove_optional_attribute_quotes=False),
              file=new_file)


def write_text(file_name):
    template_name = file_name.replace(".mustache.txt", "")
    with open("../" + template_name + ".txt.mustache", "w") as new_file:
        text = container_text.replace("{{>body}}", open(template_name + ".mustache.txt").read())
        print(text, file=new_file)


css_content = read_branding(css_content)

# Replace variables in css style, using _custom_palette.scss

container_html = container_html.replace("{{>style_css}}", css_content)

for file in os.listdir("."):
    if file.endswith(".mustache.html"):
        write_html(file)

    if file.endswith(".mustache.txt"):
        write_text(file)


shutil.copyfile("notifications-language.json", "../notifications-language.json")

"""
for f in ("camera_input", "camera_ip_conflict", "camera_motion", "mediaserver_conflict", "generic_event", "mediaserver_failure", "mediaserver_started", "storage_failure", "license_issue"):

    """

