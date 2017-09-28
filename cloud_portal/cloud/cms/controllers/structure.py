import base64
import json
import codecs
import os
from zipfile import ZipFile
from cloud import settings
from ..models import Product, Context, DataStructure, Customization, DataRecord, customization_cache


def find_or_add_product(name):
    if Product.objects.filter(name=name).exists():
        return Product.objects.get(name=name)
    product = Product(name=name)
    product.save()
    return product


def find_or_add_context(context_name, old_name, product_id, has_language, is_global):
    if old_name and Context.objects.filter(name=old_name, product_id=product_id).exists():
        context = Context.objects.get(name=old_name, product_id=product_id)
        context.name = context_name
        context.save()
        return context

    if Context.objects.filter(name=context_name, product_id=product_id).exists():
        return Context.objects.get(name=context_name, product_id=product_id)

    context = Context(name=context_name, file_path=context_name,
                      product_id=product_id, translatable=has_language,
                      is_global=is_global)
    context.save()
    return context


def find_or_add_data_structure(name, old_name, context_id, has_language):
    if old_name and DataStructure.objects.filter(name=old_name, context_id=context_id).exists():
        record = DataStructure.objects.get(name=old_name, context_id=context_id)
        record.name = name
        record.save()
        return record

    if DataStructure.objects.filter(name=name, context_id=context_id).exists():
        return DataStructure.objects.get(name=name, context_id=context_id)
    data = DataStructure(name=name, context_id=context_id,
                         translatable=has_language, default=name)
    data.save()
    return data


def update_from_object(cms_structure):
    product_name = cms_structure['product']
    default_language = customization_cache('default', 'default_language')
    product_id = find_or_add_product(product_name).id
    order = 0

    for context_data in cms_structure['contexts']:
        has_language = context_data["translatable"]
        is_global = context_data["is_global"] if "is_global" in context_data else False
        old_name = context_data["old_name"] if "old_name" in context_data else None
        context = find_or_add_context(
            context_data["name"], old_name, product_id, has_language, is_global)
        if "description" in context_data:
            context.description = context_data["description"]
        if "file_path" in context_data:
            context.file_path = context_data["file_path"]
        if "url" in context_data:
            context.url = context_data["url"]
        context.is_global = is_global
        context.hidden = context_data["hidden"] if "hidden" in context_data else False
        context.save()

        for record in context_data["values"]:
            if not isinstance(record, dict):
                old_name = None
                description = None
                record_type = "text"
                meta = None
                advanced = False
                if len(record) == 3:
                    label, name, value = record
                if len(record) == 4:
                    label, name, value, description = record
            else:
                label = record['label']
                name = record['name']
                value = record['value']
                old_name = record['old_name'] if 'old_name' in record else None
                description = record['description'] if 'description' in record else None
                record_type = record['type'] if 'type' in record else None
                meta = record['meta'] if 'meta' in record else None
                advanced = record['advanced'] if 'advanced' in record else False

            data_structure = find_or_add_data_structure(name, old_name, context.id, has_language)

            data_structure.order = order
            order += 1
            data_structure.label = label if label else name
            data_structure.advanced = advanced
            if description:
                data_structure.description = description
            if record_type:
                data_structure.type = DataStructure.get_type(record_type)

            if data_structure.type == DataStructure.DATA_TYPES.image:
                data_structure.translatable = "{{language}}" in name

                # this is used to convert source images into b64 strings
                file_path = os.path.join('static', 'default', 'source', name)
                file_path = file_path.replace("{{language}}", default_language)
                try:
                    with open(file_path, 'r') as file:
                        value = base64.b64encode(file.read())
                except IOError:
                    print("No file to read", file_path)

            data_structure.meta_settings = meta if meta else {}
            data_structure.default = value
            data_structure.save()


def read_structure_json(filename):
    with codecs.open(filename, 'r', 'utf-8') as file_descriptor:
        cms_structure = json.load(file_descriptor)
        update_from_object(cms_structure)


def process_zip(file_descriptor, user, update_structure, update_defaults, update_content):
    zip_file = ZipFile(file_descriptor)
    # zip_file.printdir()
    root = None

    if update_structure:
        name = next((name for name in zip_file.namelist() if name.endswith('structure.json')), None)
        if name:
            data = zip_file.read(name)
            cms_structure = json.loads(data)
            update_from_object(cms_structure)

    for name in zip_file.namelist():
        if name.startswith('__'):  # Ignore trash in archive from MACs
            continue

        if name.endswith('structure.json'):  # Ignore **structure.json files
            continue

        if name.endswith('/'):
            if not root:  # find root directory to ignore
                root = name
            continue  # not a file - ignore it

        short_name = name.replace(root, '')

        if short_name.startswith('help/'):  # Ignore help
            continue

        # now we have name

        # try to find relevant context and update its template
        if update_structure:
            context = Context.objects.filter(file_path=short_name)
            if context.exists():
                context.template = zip_file.read(name)
                context.save()
                continue

        # try to find relevant data structure and update its default (maybe)
        structure = DataStructure.objects.filter(name=short_name)
        if not structure.exists():
            print("NOT EXISTS", short_name)
            continue
        structure = structure.first()

        # if data structure is not FILE or IMAGE - print to log and ignore
        if structure.type not in (DataStructure.DATA_TYPES.image, DataStructure.DATA_TYPES.file):
            print("NOT FILE", short_name)
            continue

        data = zip_file.read(name)
        data64 = base64.b64encode(data)

        if update_defaults:
            # if set_defaults or data structure has no default value - save it
            structure.default = data64
            structure.save()

        if update_content:
            customization = Customization.objects.get(name=settings.CUSTOMIZATION)
            # get latest value
            latest_value = structure.find_actual_value(customization)
            # check if file was changed
            if latest_value == data64:
                print("not changed", short_name)
                continue
            print("add new record", short_name)
            # add new dataRecrod
            record = DataRecord(
                data_structure=structure,
                customization=customization,
                value=data64,
                created_by=user
            )
            record.save()
