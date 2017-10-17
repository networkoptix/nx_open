# python script generates structure.json based on directory or archive
import os
import re
import io
from collections import OrderedDict
from PIL import Image  # get Pillow
from zipfile import ZipFile
from ..models import DataStructure, Context

IGNORE_DIRECTORIES = ('help',)
IMAGES_EXTENSIONS = ('ico', 'png', 'bmp', 'icns')


def image_meta(data, extension):
    with Image.open(io.BytesIO(data)) as img:
        width, height = img.size
    return OrderedDict([('width', width), ('height', height), ('format', extension)])


def find_context(name, file_path, structure, product_name):
    context = next((context for context in structure["contexts"]
                   if context["name"] == name or file_path and context["file_path"] == file_path), None)
    if not context:
        db_context = Context.objects.filter(file_path=file_path, product__name=product_name)
        translatable = False
        hidden = True
        description = ""
        if db_context.exists():
            db_context = db_context.first()
            name = db_context.name
            description = db_context.description
            translatable = db_context.translatable
            hidden = db_context.hidden

        context = OrderedDict([
            ("name", name),
            ("description", description),
            ("file_path", file_path),
            ("translatable", translatable),
            ("hidden", hidden),
            ("values", [])
        ])
        structure["contexts"].append(context)
    return context


def find_structure(name, context, structure_type, meta=None, description=""):
    data_structure = next((structure for structure in context["values"] if structure["name"] == name), None)
    if not data_structure:
        # try to populate structure from database
        db_structure = DataStructure.objects.filter(name=name)
        label = ''
        value = ''
        if db_structure.exists():
            db_structure = db_structure.first()
            label = db_structure.label if db_structure.label != name else ''
            value = db_structure.default
            if db_structure.description:
                description = db_structure.description
            if db_structure.type:
                structure_type = DataStructure.DATA_TYPES[db_structure.type]

        data_structure = OrderedDict([
            ("label", label),
            ("name", name),
            ("value", value),
            ("description", description),
            ("type", structure_type)
        ])
        if meta:
            data_structure.update({"meta": meta})
        context["values"].append(data_structure)

    return data_structure


def read_cms_strings(data):
    pattern = re.compile(r'^(.*(%\S+?%).*)$', re.MULTILINE)
    # with open(filename, 'r') as file:
    try:
        data = data.decode('utf-8')
        return re.findall(pattern, data)
    except UnicodeDecodeError:
        return None


def check_if_customizable(data, short_name, structure, product_name):
    strings = read_cms_strings(data)
    if not strings:
        return False

    # customizable file creates new structure
    context = find_context(short_name, short_name, structure, product_name)
    for match in strings:
        find_structure(match[1], context, 'Text', description=match[0])
    return True


def read_data(data, short_name, context, cms_structure, product_name):
    extension = os.path.splitext(short_name)[1][1:]
    if short_name.endswith('DS_Store'):
        return
    meta = OrderedDict(format=extension)
    structure_type = 'File'
    if extension in IMAGES_EXTENSIONS:
        meta = image_meta(data, extension)
        structure_type = 'image'
    elif check_if_customizable(data, short_name, cms_structure, product_name):
        return
    find_structure(short_name, context, structure_type, meta=meta)


def iterate_zip(file_descriptor):
    zip_file = ZipFile(file_descriptor)
    root = None
    for name in zip_file.namelist():
        if name.count('/') == 0:  #  ignore files from the root of the archive
            print("IGNORED FILE", name)
            continue
        if name.endswith('/'):
            if not root:  # find root directory to ignore
                root = name
                continue
            yield name.replace(root, ''), None
        else:
            yield name.replace(root, ''), zip_file.read(name)


def iterate_directory(directory):
    for root, dirs, files in os.walk(directory):
        print(root.replace(directory, '') + '/')
        yield root.replace(directory, '') + '/', None
        for filename in files:
            filename = os.path.join(root, filename)
            with open(filename, "rb") as file_descriptor:
                data = file_descriptor.read()
                print(filename.replace(directory, ''))
                yield filename.replace(directory, ''), data


def iterate_contexts(file_iterator):
    root = None
    context_name = 'root'
    for name, data in file_iterator:
        if name.startswith('__'):  # Ignore trash in archive from MACs
            continue

        if name.endswith('structure.json'):  # Ignore **structure.json files
            continue

        root_dir = name.split('/')[0]
        if root_dir in IGNORE_DIRECTORIES:
            continue

        if name.endswith('/'):  # this is directory
            if name.count('/') == 1:  # top level directory - update active_context
                context_name = name
            continue  # ignore directories

        if name.count('/') == 1:  # file from top level directory - update active_context
            context_name = 'root'

        yield name, context_name, data


def process_files(file_iterator, product_name):
    structure = OrderedDict([('product', product_name), ('contexts', [])])
    root_context = find_context('root', '.', structure, product_name)
    for short_name, context_name, data in iterate_contexts(file_iterator):
        context = find_context(context_name, context_name, structure, product_name)
        read_data(data, short_name, context, structure, product_name)
    return structure


def from_zip(file_descriptor, product_name):
    return process_files(iterate_zip(file_descriptor), product_name)


def from_directory(directory, product_name):
    return process_files(iterate_directory(directory), product_name)
