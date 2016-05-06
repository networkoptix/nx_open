import os
import re
import xml.etree.ElementTree as eTree
import yaml
from os.path import join


# 1. read ts into xml
xml_files = ('cloud_portal.ts', 'templates.ts')
root_directory = ''

# Read branding info

branding_messages = {}


def get_config():
    conf_dir = os.getenv('CLOUD_PORTAL_CONF_DIR')
    if not conf_dir:
        raise RuntimeError('CLOUD_PORTAL_CONF_DIR environment variable is undefined')

    return yaml.safe_load(open(join(conf_dir, 'cloud_portal.yaml')))

def read_branding():
    print "read_branding"
    conf = get_config()
    customization = conf['customization']
    branding_file = '../../customizations/' + customization + '/branding.ts'
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
        # proceed with branding
        active_content = process_branding(content)

        # save old file
        with open(filename, "w") as file_descriptor:
            print "save: " + filename
            file_descriptor.write(active_content)

def process_files():
    for xml_file in xml_files:
        tree = eTree.parse(xml_file)
        root = tree.getroot()

        # 2. go file by file
        active_filename = ''
        active_content = ''
        for context in root.iter('context'):
            for message in context.iter('message'):
                location = message.find('location')
                if location is not None and location.get('filename'):
                    save_content(active_filename,active_content)
                    active_filename = os.path.join(root_directory, location.get('filename'))
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
        save_content(active_filename, active_content)


read_branding()
process_files()
