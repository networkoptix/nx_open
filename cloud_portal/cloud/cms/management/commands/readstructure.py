# read source folder
# find all cms templates (%...%)
# update database structure
# mark everything in the database which was not found in sources
# create report: added vs outdated
import os
import re
import base64
from ...controllers import filldata
from cloud import settings
import json
import codecs
from ...models import Product, Context, DataStructure
from django.core.management.base import BaseCommand

#  from cms.controllers.readstructure import *


def find_or_add_context_by_file(file_path, product_id, has_language):
    if Context.objects.filter(file_path=file_path, product_id=product_id).exists():
        return Context.objects.get(file_path=file_path, product_id=product_id)
    context = Context(name=file_path, file_path=file_path,
                      product_id=product_id, translatable=has_language,
                      is_global=False)
    context.save()
    return context


def find_or_add_context(context_name, product_id, has_language, is_global):
    if Context.objects.filter(name=context_name, product_id=product_id).exists():
        return Context.objects.get(name=context_name, product_id=product_id)
    context = Context(name=context_name, file_path=context_name,
                      product_id=product_id, translatable=has_language,
                      is_global=is_global)
    context.save()
    return context


def find_or_add_data_stucture(name, context_id, has_language):
    if DataStructure.objects.filter(name=name, context_id=context_id).exists():
        return DataStructure.objects.get(name=name, context_id=context_id)
    data = DataStructure(name=name, context_id=context_id,
                         translatable=has_language, default=name)
    data.save()
    return data


def read_cms_strings(filename):
    pattern = re.compile(r'%\S+%')
    with open(filename, 'r') as file:
        data = file.read()
        return re.findall(pattern, data)


def read_structure_file(filename, product_id):
    context_name, language = filldata.context_for_file(filename, 'default')

    if language and language != "en_US":
        return
    # now read file and get records from there.
    strings = read_cms_strings(filename)

    if strings:     # if there is no records at all - we ignore it
        context = find_or_add_context_by_file(
            context_name, product_id, bool(language))
        for string in strings:
            find_or_add_data_stucture(string, context.id, bool(language))


def read_structure():
    product_id = Product.objects.get(name='cloud_portal').id
    for file in filldata.iterate_cms_files('default', True):
        read_structure_file(file, product_id)


def read_structure_json():
    with codecs.open('cms/cms_structure.json', 'r', 'utf-8') as file_descriptor:
        cms_structure = json.load(file_descriptor)
    product_name = cms_structure['product']
    product_id = Product.objects.get(name=product_name).id
    for context_data in cms_structure['contexts']:
        has_language = context_data["translatable"]
        is_global = False
        if "is_global" in context_data:
            is_global = context_data["is_global"]
        context = find_or_add_context(
            context_data["name"], product_id, has_language, is_global)
        if "description" in context_data:
            context.description = context_data["description"]
        if "file_path" in context_data:
            context.file_path = context_data["file_path"]
        if "url" in context_data:
            context.url = context_data["url"]
        context.save()

        for record in context_data["values"]:
            description = None
            type = None
            meta = None
            advanced = False
            if len(record) == 3:
                label, name, value = record
            if len(record) == 4:
                label, name, value, description = record
            if len(record) > 4:
                label = record['label']
                name = record['name']
                value = record['value']

                if 'description' in record:
                    description = record['description']

                if 'type' in record:
                    type = record['type']

                if 'meta' in record:
                    meta = record['meta']

                if 'advanced' in record:
                    advanced = record['advanced']

            data_structure = find_or_add_data_stucture(
                name, context.id, has_language)
            print(data_structure.advanced)
            data_structure.label = label
            data_structure.advanced = advanced
            if description:
                data_structure.description = description
            if type:
                data_structure.type = DataStructure.get_type(type)

            if type and type == "Image":
                data_structure.translatable = "{{language}}" in name

                #this is used to convert source images into b64 strings
                file_path = os.path.join('static', 'default', 'source', name)
                file_path = file_path.replace("{{language}}", settings.DEFAULT_LANGUAGE)
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
