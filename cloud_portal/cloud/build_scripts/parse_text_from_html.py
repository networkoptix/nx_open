import os
import re

file_root_dir = '../static/views'
file_filter = '.html'


def process_file(filename):
    with open(filename, 'r') as myfile:
        data = myfile.read()
        # 1. collapse html tags
        data = re.sub(r'<.+?>', '<>', data)

        data = re.sub(r'\{\{.+?\}\}', '', data)

        data = re.sub(r'>\s*<', '', data)
        data = re.sub(r'><', '', data)

        data = re.sub(r'^<', '', data)
        data = re.sub(r'>$', '', data)
        data = re.sub(r'<>', '<\n\t>', data)

        if data <> '':
            print(filename)
            print(data)


for root, subdirs, files in os.walk(file_root_dir):
    for filename in files:
        if filename.endswith(file_filter):
            process_file(os.path.join(root, filename))

