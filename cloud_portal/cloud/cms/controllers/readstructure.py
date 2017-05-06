# read source folder
# find all cms templates (%...%)
# update database structure
# mark everything in the database which was not found in sources
# create report: added vs outdated
import os
import re
import filldata
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
        print '-------------------------------'
        print context_name
        print strings

        context = find_or_add_context(context_name, product_id, bool(language))
        for string in strings:
            find_or_add_data_stucture(string, context.id, bool(language))


def read_structure():
    product_id = Product.objects.get(name='cloud_portal').id
    for file in filldata.iterate_cms_files('default'):
        read_structure_file(file, product_id)
