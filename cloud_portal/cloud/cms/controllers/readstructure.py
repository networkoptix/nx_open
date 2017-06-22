# read source folder
# find all cms templates (%...%)
# update database structure
# mark everything in the database which was not found in sources
# create report: added vs outdated
import os
import re
import filldata
import json, codecs
from ..models import Product, Context, DataStructure

#  from cms.controllers.readstructure import *


def find_or_add_context(context_name, product_id, has_language):
    if Context.objects.filter(name=context_name, product_id=product_id).exists():
        return Context.objects.get(name=context_name, product_id=product_id)
    context = Context(name=context_name, file_path=context_name, product_id=product_id, translatable=has_language)
    context.save()
    return context


def find_or_add_data_stucture(name, context_id, has_language):
    if DataStructure.objects.filter(name=name, context_id=context_id).exists():
        return DataStructure.objects.get(name=name, context_id=context_id)
    data = DataStructure(name=name, context_id=context_id, translatable=has_language, default=name)
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
        context = find_or_add_context(context_name, product_id, bool(language))
        for string in strings:
            find_or_add_data_stucture(string, context.id, bool(language))


def read_structure():
    product_id = Product.objects.get(name='cloud_portal').id
    for file in filldata.iterate_cms_files('default'):
        read_structure_file(file, product_id)


def read_structure_json():
    with codecs.open('cms/cms_structure.json', 'r', 'utf-8') as file_descriptor:
        cms_structure = json.load(file_descriptor)
    product_name = cms_structure['product']
    product_id = Product.objects.get(name=product_name).id
    for context_data in cms_structure['contexts']:
        has_language = context_data["translatable"]
        context = find_or_add_context(context_data["name"], product_id, has_language)
        if "description" in context_data:
            context.description = context_data["description"]
        if "file_path" in context_data:
            context.file_path = context_data["file_path"]
        if "url" in context_data:
            context.url = context_data["url"]
            context.save()

        for record in context_data["values"]:
            if len(record) == 2:
                name, value = record
            if len(record) == 3:
                name, value, description = record
            if len(record) == 4:
                name, value, description, type = record

            data_structure = find_or_add_data_stucture(name, context.id, has_language)
            if description:
                data_structure.description = description
            if type:
                data_structure.type = type
            data_structure.default = value
            data_structure.save()
