# python script generates structure.json based on directory

'''
https://networkoptix.atlassian.net/browse/CLOUD-1400:

Scripts reads directory, iterates through every file and adds them to context:
If file is image - add it to a special context 'graphics', specify strictly its format, width and height
if file is text - add a separate context and try to parse it for CMS tags %...% (just like readstructure does)
otherwise - add file as file to a special context 'static'
Output must contain well-formatted json with all data + plus empty values for label, value, description for every record (for human to be able to easily fill that data in)
If that directory already has cms_structure.json file - then script should merge two of them using names as keys
'''


# 1. iterate over every directory and file in work directory
# 2. create context for each
import os
import codecs
import json
from collections import OrderedDict
from PIL import Image  # get Pillow

DIRECTORY = '../../../customization/default/'
IGNORE = ('help',)
STRUCTURE_FILE = 'vms_structure.json'
IMAGES_EXTENSIONS = ('ico', 'png', 'bmp', 'icns')
PRODUCT_NAME = 'vms'


def image_meta(filepath):
    with Image.open(filepath) as img:
        width, height = img.size
    extension = os.path.splitext(filepath)[1]
    return OrderedDict(width=width, height=height, format=extension)


def load_structure(filename):
    structure = {'product': PRODUCT_NAME, 'contexts': []}
    try:
        with codecs.open(filename, 'r', 'utf-8') as file_descriptor:
            structure = json.load(file_descriptor, object_pairs_hook=OrderedDict)
    except IOError:
        print("No existing file, create new one")
    except ValueError:
        print("Malformatted file, create new one")
    return structure


def save_structure(filename, structure):
    content = json.dumps(structure, ensure_ascii=False, indent=4, separators=(',', ': '))
    with codecs.open(filename, "w", "utf-8") as file:
        file.write(content)


def find_context(name, file_path, structure):
    context = next((context for context in structure["contexts"]
                   if context["name"] == name or file_path and context["file_path"] == file_path), None)
    if not context:
        context = OrderedDict([
            ("name", name),
            ("description", ""),
            ("file_path", file_path),
            ("translatable", False),
            ("values", [])
        ])
        structure["contexts"].append(context)
    return context


def find_structure(name, context, type, meta):
    data_structure = next((structure for structure in context["values"] if structure["name"] == name), None)
    if not data_structure:
        data_structure = OrderedDict([
            ("label", ""),
            ("name", name),
            ("value", ""),
            ("description", ""),
            ("type", type),
            ("meta", meta)
        ])
        context["values"].append(data_structure)

    return data_structure


cms_structure = load_structure(STRUCTURE_FILE)

for name in os.listdir(DIRECTORY):
    if name in IGNORE:
        continue
    context_path = os.path.join(DIRECTORY, name)
    if os.path.isdir(context_path):
        context = find_context(name, None, cms_structure)

        for root, dirs, files in os.walk(context_path):
            # print(root, dirs, files)
            cutroot = root.replace(DIRECTORY, '')
            for filename in files:
                filepath = os.path.join(root, filename)
                cutpath = os.path.join(cutroot, filename)
                extension = os.path.splitext(filename)[1][1:]

                meta = OrderedDict(format=extension)
                type = 'File'
                if extension in IMAGES_EXTENSIONS:
                    meta = image_meta(filepath)
                    type = 'Image'

                find_structure(cutpath, context, type, meta)
                pass


save_structure(STRUCTURE_FILE, cms_structure)
