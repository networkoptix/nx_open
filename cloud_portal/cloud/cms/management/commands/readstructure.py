# read source folder
# find all cms templates (%...%)
# update database structure
# mark everything in the database which was not found in sources
# create report: added vs outdated
import os
import re
import base64
from ...controllers import filldata
import json
import codecs
from ...models import Product, Context, DataStructure, customization_cache
from django.core.management.base import BaseCommand

SOURCE_DIR = 'static/{{customization}}/source/'


def customizable_file(filename, ignore_not_english):
    supported_format = filename.endswith('.json') or \
        filename.endswith('.html') or \
        filename.endswith('.mustache') or \
        filename.endswith('apple-app-site-association')
    supported_directory = not ignore_not_english or \
        "lang_" not in filename or "lang_en_US" in filename
    return supported_format and supported_directory


def iterate_cms_files(customization_name, ignore_not_english):
    custom_dir = SOURCE_DIR.replace("{{customization}}", customization_name)
    for root, dirs, files in os.walk(custom_dir):
        for filename in files:
            file = os.path.join(root, filename)
            if customizable_file(file, ignore_not_english):
                yield file


def find_or_add_context_by_file(file_path, product_id, has_language):
    if Context.objects.filter(file_path=file_path, product_id=product_id).exists():
        return Context.objects.get(file_path=file_path, product_id=product_id)
    context = Context(name=file_path, file_path=file_path,
                      product_id=product_id, translatable=has_language,
                      hidden=True,
                      is_global=False)
    context.save()
    return context


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


def find_or_add_data_stucture(name, old_name, context_id, has_language):
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


def read_cms_strings(filename):
    pattern = re.compile(r'%\S+?%')
    with open(filename, 'r') as file:
        data = file.read()
        return set(re.findall(pattern, data))


def read_structure_file(filename, product_id, global_strings):
    context_name, language = filldata.context_for_file(filename, 'default')

    if language and language != "en_US":
        return
    # now read file and get records from there.
    strings = read_cms_strings(filename)
    if not strings:  # if there is no records at all - we ignore it
        return

    # now, here this is customization-depending file

    strings = [string for string in strings if string not in global_strings]
    context = find_or_add_context_by_file(
        context_name, product_id, bool(language))

    for string in strings:
        # Here we need to check if there are any unique strings (which are not global
        find_or_add_data_stucture(string, None, context.id, bool(language))


def read_structure():
    product_id = Product.objects.get(name='cloud_portal').id
    global_strings = DataStructure.objects.\
        filter(context__is_global=True, context__product_id=product_id).\
        values_list("name", flat=True)
    for file in iterate_cms_files('default', True):
        read_structure_file(file, product_id, global_strings)


def read_structure_json():
    with codecs.open('cms/cms_structure.json', 'r', 'utf-8') as file_descriptor:
        cms_structure = json.load(file_descriptor)
    product_name = cms_structure['product']
    default_language = customization_cache('default', 'default_language')
    product_id = Product.objects.get(name=product_name).id
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
        context.hidden = False
        context.save()

        for record in context_data["values"]:
            if not isinstance(record, dict):
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

            data_structure = find_or_add_data_stucture(
                name, old_name, context.id, has_language)

            data_structure.label = label
            data_structure.advanced = advanced
            if description:
                data_structure.description = description
            if record_type:
                data_structure.type = DataStructure.get_type(record_type)

            if type and type == "Image":
                data_structure.translatable = "{{language}}" in name

                #this is used to convert source images into b64 strings
                file_path = os.path.join('static', 'default', 'source', name)
                file_path = file_path.replace("{{language}}", default_language)
                with open(file_path, 'r') as file:
                    value = encoded_string = base64.b64encode(file.read())
                        
            data_structure.meta_settings = meta if meta else {}
            data_structure.default = value
            data_structure.save()


class Command(BaseCommand):
    help = 'Creates initial structure for CMS in\
     the database (contexts, datastructure)'

    def handle(self, *args, **options):
        read_structure_json()
        read_structure()
        self.stdout.write(self.style.SUCCESS(
            'Successfully initiated data structure for CMS'))
