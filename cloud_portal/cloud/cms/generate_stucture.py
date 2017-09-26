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
import re
import json
from collections import OrderedDict
from PIL import Image  # get Pillow

DIRECTORY = '../../../customization/default/'
STRUCTURE_FILE = 'vms_structure.json'
PRODUCT_NAME = 'vms'

IGNORE_DIRECTORIES = ('help',)
IGNORE_FORMATS = ('DS_Store',)
IMAGES_EXTENSIONS = ('ico', 'png', 'bmp', 'icns')


def image_meta(filepath, extension):
    with Image.open(filepath) as img:
        width, height = img.size
    return OrderedDict([('width', width), ('height', height), ('format', extension)])


def load_structure(filename, ignore=True):
    structure = OrderedDict([('product', PRODUCT_NAME), ('contexts', [])])

    if not ignore:
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


def find_structure(name, context, structure_type, meta=None, description=""):
    data_structure = next((structure for structure in context["values"] if structure["name"] == name), None)
    if not data_structure:
        data_structure = OrderedDict([
            ("label", ""),
            ("name", name),
            ("value", ""),
            ("description", description),
            ("type", structure_type)
        ])
        if meta:
            data_structure.update({"meta": meta})
        context["values"].append(data_structure)

    return data_structure


def read_cms_strings(filename):
    pattern = re.compile(r'^(.*(%\S+?%).*)$', re.MULTILINE)
    # with open(filename, 'r') as file:
    try:
        with codecs.open(filename, 'r', 'utf-8') as file:
            data = file.read()
            # if 'html' in filename:
            #    print(data)
            return re.findall(pattern, data)
    except UnicodeDecodeError:
        return None


def check_customizable(file_path, root, structure):
    strings = read_cms_strings(file_path)
    if not strings:
        return False

    # customizable file creates new structure
    context = find_context(file_path.replace(root, ''), file_path.replace(root, ''), structure)
    for match in strings:
        find_structure(match[1], context, 'Text', description=match[0])

    return True


def read_file(root, directory, filename, context=None):
    file_path = os.path.join(directory, filename)
    short_path = file_path.replace(root, '')
    extension = os.path.splitext(filename)[1][1:]
    if extension in IGNORE_FORMATS:
        return
    meta = OrderedDict(format=extension)
    structure_type = 'File'
    if extension in IMAGES_EXTENSIONS:
        meta = image_meta(file_path, extension)
        structure_type = 'Image'
    elif check_customizable(file_path, root, cms_structure):
        return
    find_structure(short_path, context, structure_type, meta=meta)


def read_root_directory(directory, structure):
    root_context = find_context('root', '.', structure)
    for name in os.listdir(directory):
        if name in IGNORE_DIRECTORIES:
            continue
        context_path = os.path.join(directory, name)
        if os.path.isdir(context_path):
            context = find_context(name, name, structure)
            for root, dirs, files in os.walk(context_path):
                for filename in files:
                    read_file(directory, root, filename, context)
        else:
            read_file(directory, directory, name, root_context)


cms_structure = load_structure(STRUCTURE_FILE)
read_root_directory(DIRECTORY, cms_structure)
save_structure(STRUCTURE_FILE, cms_structure)
