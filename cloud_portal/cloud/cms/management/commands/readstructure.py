# read source folder
# find all cms templates (%...%)
# update database structure
# mark everything in the database which was not found in sources
# create report: added vs outdated
import os
import re
from ...controllers import filldata, structure
from ...models import Product, Context, DataStructure
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

    # Here we check if there are any unique strings (which are not global)
    strings = [string for string in strings if string not in global_strings]
    context = find_or_add_context_by_file(
        context_name, product_id, bool(language))

    for string in strings:
        structure.find_or_add_data_structure(string, None, context.id, bool(language))


def read_structure(product_name):
    product_id = Product.objects.get(name=product_name).id
    global_strings = DataStructure.objects.\
        filter(context__is_global=True, context__product_id=product_id).\
        values_list("name", flat=True)
    for file in iterate_cms_files('default', True):
        read_structure_file(file, product_id, global_strings)


class Command(BaseCommand):
    help = 'Creates initial structure for CMS in ' \
           'the database (contexts, datastructure)'

    def handle(self, *args, **options):
        structure.read_structure_json('cms/cms_structure.json')
        read_structure('cloud_portal')
        try:
            structure.read_structure_json('cms/vms_structure.json')
        except IOError:
            print("No vms_structure to read")
        self.stdout.write(self.style.SUCCESS(
            'Successfully initiated data structure for CMS'))
