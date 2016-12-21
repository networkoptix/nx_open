import os
import re
import xml.etree.ElementTree as eTree
import yaml
from os.path import join

import errno
import yaml

# 1. read ts into xml
xml_files = (('static', 'cloud_portal.ts'), ('templates', 'cloud_templates.ts'))

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

        # save old file
        with open(filename, "w") as file_descriptor:
            print "save: " + filename
            file_descriptor.write(active_content)


def process_files(lang):
    for xml_file in xml_files:
        root_directory = xml_file[0]
        tree = eTree.parse(xml_file[1])
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
                    target_filename = os.path.join(root_directory, lang, filename)

                    with open(active_filename, 'r') as file_descriptor:
                        active_content = file_descriptor.read()

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
    process_files(lang)
