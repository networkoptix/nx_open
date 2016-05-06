import os
import re
import xml.etree.ElementTree as eTree

# 1. read ts into xml
xml_files = ('cloud_portal.ts', 'templates.ts')
branding_file = '../../customizations/default/branding.ts'
root_directory = ''

# Read branding info

branding_messages = {}

def read_branding():
    tree = eTree.parse(branding_file)
    root = tree.getroot()

    for context in root.iter('context'):
        name = context.find('name')
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
                    if active_filename:
                        # proceed with branding
                        active_content = process_branding(active_content)

                        # save old file
                        with open(active_filename, "w") as file_descriptor:
                            print("save:", active_filename)
                            file_descriptor.write(active_content)

                    active_filename = os.path.join(root_directory, location.get('filename'))

                    with open(active_filename, 'r') as file_descriptor:
                        print("read:", active_filename)
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

read_branding()
process_files()
