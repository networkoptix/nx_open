import os
import re

file_root_dir = '../static/scripts'
file_filter = 'language.js'


def process_file(filename):
    with open(filename, 'r') as myfile:

        data = myfile.read()

        result = re.findall("'.+?'",data)

        # 1. collapse html tags

        print("\n\n\n\n\n\n\n\n")
        print(filename)
        for res in result:
            print(res)


for root, subdirs, files in os.walk(file_root_dir):
    for filename in files:
        if filename.endswith(file_filter):
            process_file(os.path.join(root, filename))

