import os
import xml.etree.ElementTree as eTree

# 1. read ts into xml
xml_file = 'cloud_portal.ts'
root_directory = ''

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
                # save old file
                with open(active_filename, "w") as file_descriptor:
                    file_descriptor.write(active_content)

            active_filename = os.path.join(root_directory, location.get('filename'))

            with open(active_filename, 'r') as file_descriptor:
                active_content = file_descriptor.read()

        source = message.find('source').text

        translation = message.find('translation').text

        if translation and translation.strip():
            # 3. replace string in file

            print("replacing:", active_filename, source, translation)
            active_content = active_content.replace(source, translation)
