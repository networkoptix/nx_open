# python script generates structure.json based on directory or archive
import os
import re
import io
from collections import OrderedDict
from PIL import Image  # get Pillow
from zipfile import ZipFile
from ..models import Context, DataStructure, ProductType
from cms.serializers import ProductTypeSerializer

import logging
logger = logging.getLogger(__name__)

IGNORE_DIRECTORIES = ('help',)
IMAGES_EXTENSIONS = ('ico', 'png', 'bmp', 'icns', 'jpg', 'jpeg')


class RecordState(object):
    deprecated = "deprecated"
    new = "new"
    same = "same"
    updated = "updated"


REC_STATE = RecordState()


def image_meta(data, extension):
    # noinspection PyBroadException
    try:
        with Image.open(io.BytesIO(data)) as img:
            width, height = img.size
        return OrderedDict([('width', width), ('height', height), ('format', extension)])
    except:
        return None


def find_context(name, file_path, structure, product_name):
    context = next((context for context in structure["contexts"]
                   if context["name"] == name or file_path and context["file_path"] == file_path), None)
    if not context:
        db_context = Context.objects.filter(file_path=file_path, product__name=product_name).first()
        translatable = False
        hidden = True
        label = ""
        description = ""
        if db_context:
            name = db_context.name
            label = db_context.label
            description = db_context.description
            translatable = db_context.translatable
            hidden = db_context.hidden

        context = OrderedDict([
            ("name", name),
            ("label", label),
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
        db_structure = DataStructure.objects.filter(name=name).first()
        label = ''
        value = ''
        if db_structure:
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
            ("type", structure_type),
            ("advanced", False),
            ("optional", False),
            ("public", True)
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
    structure_type = 'file'
    if extension in IMAGES_EXTENSIONS:
        meta = image_meta(data, extension)
        if not meta:
            return {'file': short_name, 'extension': extension}
        structure_type = 'image'
    elif check_if_customizable(data, short_name, cms_structure, product_name):
        return
    find_structure(short_name, context, structure_type, meta=meta)


def iterate_zip(file_descriptor):
    zip_file = ZipFile(file_descriptor)
    root = ''
    for name in zip_file.namelist():
        if name.count('/') == 0:  # ignore files from the root of the archive
            logger.info(f"IGNORED FILE {name}")
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
        logger.info(f"{root.replace(directory, '')}/")
        yield root.replace(directory, '') + '/', None
        for filename in files:
            filename = os.path.join(root, filename)
            with open(filename, "rb") as file_descriptor:
                data = file_descriptor.read()
                logger.info(f"{filename.replace(directory, '')}")
                yield filename.replace(directory, ''), data


def iterate_contexts(file_iterator):
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
                context_name = root_dir
            continue  # ignore directories

        if name.count('/') == 0:  # file from top level directory - update active_context
            context_name = 'root'

        yield name, context_name, data


def get_object_by_name(name, objects):
    return next((obj for obj in objects if name == obj["name"]), None)


def list_to_dict(obj, key):
    return {o[key]: o for o in obj}


def merge_object(obj1, obj2, obj_join, state=REC_STATE.same, parent_key=""):
    for key in obj1:
        if key == "status":
            continue

        if key not in obj2:
            state = REC_STATE.updated
            del obj_join[key]
            continue

        if type(obj1[key]) is dict:
            state = merge_object(obj1[key], obj2[key], obj_join[key], state, key)

        elif obj1[key] != obj2[key]:
            obj_join[key] = obj2[key]
            state = REC_STATE.updated

    for key in obj2:
        if key == "status":
            continue

        if key not in obj1:
            obj_join[key] = obj2[key]
            state = REC_STATE.updated

    if parent_key != "meta":
        obj_join["status"] = state

    return state


def merge_context(base_context, new_context):
    merged_data_structures = base_context["values"]
    base_data_structures = list_to_dict(base_context["values"], "name")
    new_data_structures = list_to_dict(new_context["values"], "name")

    for key, base_ds in base_data_structures.items():
        # check and update matching data structures
        if key in new_data_structures:
            merge_ds = get_object_by_name(key, merged_data_structures)
            merge_object(base_ds, new_data_structures[key], merge_ds)

        else:
            ds = get_object_by_name(key, merged_data_structures)
            ds["status"] = REC_STATE.deprecated

    for key, new_ds in new_data_structures.items():
        if key not in base_data_structures:
            new_ds["status"] = REC_STATE.new
            merged_data_structures.append(new_ds)

    return merged_data_structures


def merge_structure(base_structure, new_structure):
    merged_structure = base_structure
    base_contexts = list_to_dict(base_structure["contexts"], "name")
    new_contexts = list_to_dict(new_structure["contexts"], "name")

    for key, base_context in base_contexts.items():
        # Update base
        context = get_object_by_name(key, merged_structure["contexts"])
        if key in new_contexts:
            context["values"] = merge_context(base_context, new_contexts[key])

            status = {ds["status"] for ds in context["values"]}
            context["status"] = next(iter(status)) if len(status) < 2 else REC_STATE.updated
        else:
            set_data_structure_state(context["values"], REC_STATE.deprecated)
            context["status"] = REC_STATE.deprecated

    for key, new_context in new_contexts.items():
        if key not in base_contexts:
            set_data_structure_state(new_context["values"], REC_STATE.new)
            new_context["status"] = REC_STATE.new
            merged_structure["contexts"].append(new_context["values"])

    return merged_structure


def process_files(file_iterator, product):
    log_errors = []
    structure = OrderedDict([('product', product.name),
                             ('type', ProductType.PRODUCT_TYPES[product.product_type.type]),
                             ('single_customization', product.product_type.single_customization),
                             ('can_preview', product.product_type.can_preview),
                             ('contexts', [])])
    find_context('root', '.', structure, product.name)
    for short_name, context_name, data in iterate_contexts(file_iterator):
        context = find_context(context_name, context_name, structure, product.name)
        error = read_data(data, short_name, context, structure, product.name)
        if error:
            log_errors.append(error)
    return [structure], log_errors


def from_database(product, use_actual_values=False, draft=False):
    return [ProductTypeSerializer(product.product_type, use_actual_values=use_actual_values,
                                  product=product, lang=product.default_language, draft=draft).data]


def from_directory(directory, product):
    return process_files(iterate_directory(directory), product)


def from_zip(file_descriptor, product):
    return process_files(iterate_zip(file_descriptor), product)


def merge_db_with_archive(file_descriptor, product):
    db_structure = from_database(product, use_actual_values=False)[0]
    archive_structure = from_zip(file_descriptor, product)[0][0]
    return merge_structure(db_structure, archive_structure)


def set_data_structure_state(context, state):
    for ds in context["values"]:
        ds["state"] = state
