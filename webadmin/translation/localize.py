import os
import re
import xml.etree.ElementTree as eTree
from os.path import join


# 1. read ts into xml
xml_files = ('webadmin.ts',)
root_directory = ''

# Read branding info

branding_messages = {}


def save_content(filename, content):
    if filename:
        # save old file
        with open(filename, "w") as file_descriptor:
            print "save: " + filename
            file_descriptor.write(content)


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
                    if not os.path.isfile(active_filename):  # file was removed - jump to next content
                        break
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

process_files()
