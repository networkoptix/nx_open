import os
import re
import xml.etree.ElementTree as eTree
import yaml
from os.path import join

import errno
import yaml
import shutil
import codecs

# 1. read ts into xml
# Read branding info

branding_messages = {}


def read_branding():
    print "read_branding"
    branding_file = 'branding.ts'
    tree = eTree.parse(branding_file)
    root = tree.getroot()

    for context in root.iter('context'):
        name = context.find('name').text
        if name != 'global':
            continue
        for message in context.iter('message'):
            source = message.find('source').text
            translation = message.find('translation').text
            branding_messages[source] = translation


def process_branding(content):
    for src, trans in branding_messages.iteritems():
        content = content.replace(src, trans)
    return content


def save_content(filename, content):
    if filename:
        if not os.path.exists(os.path.dirname(filename)):
            try:
                os.makedirs(os.path.dirname(filename))
            except OSError as exc:  # Guard against race condition
                if exc.errno != errno.EEXIST:
                    raise

        # proceed with branding
        active_content = process_branding(content)

        with codecs.open(filename, "w", "utf-8") as file:
            file.write(active_content)
            file.close()


def process_files(lang, root_directory, xml_filename):
    # 1. Copy all sources to target dir

    xml_filename = os.path.join("../../..", "translations", lang, xml_filename)
    tree = eTree.parse(xml_filename)

    root = tree.getroot()

    # 2. go file by file
    active_filename = ''
    target_filename = ''
    active_content = ''
    for context in root.iter('context'):
        for message in context.iter('message'):
            location = message.find('location')
            if location is not None and location.get('filename'):
                filename = location.get('filename')
                save_content(target_filename, active_content)
                active_filename = os.path.join(root_directory, filename)
                target_filename = os.path.join(root_directory, "lang_" + lang, filename)

                try:
                    with open(active_filename, 'r') as file_descriptor:
                        active_content = file_descriptor.read()
                except IOError as a:
                    print(a)
                    continue

            source = message.find('source').text

            translation = message.find('translation').text

            if translation and translation.strip():
                # 3. replace string in file
                if ' ' not in source:
                    print("! replacing single word:", source, active_filename)
                    active_content = re.sub('(?<=\W)' + re.escape(source) + '(?=\W)', translation, active_content)
                else:
                    print("replacing:", active_filename, source, translation)
                    active_content = active_content.replace(source, translation)
    save_content(target_filename, active_content)


read_branding()

# Read config - get languages there
config = yaml.safe_load(open('cloud_portal.yaml'))

# Iterate language
if 'languages' not in config:
    raise 'No languages section in cloud_portal.yaml'

languages = config['languages']
# Localize this language
for lang in languages:
    # copy static/views to target dir
    lang_dir = os.path.join('static', 'lang_' + lang)
    if os.path.isdir(lang_dir):
        shutil.rmtree(lang_dir)
    shutil.copytree(os.path.join('static', 'views'), lang_dir)

    process_files(lang, 'static', 'cloud_portal.ts')
    process_files(lang, 'templates', 'cloud_templates.ts')
