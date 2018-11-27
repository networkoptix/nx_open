# read source folder
# find all cms templates (%...%)
# update database structure
# mark everything in the database which was not found in sources
# create report: added vs outdated
import os
import re
import json
import codecs
from cloud import settings
from cloud.debug import timer
from ...controllers import structure
from ...models import *
from django.core.management.base import BaseCommand


SOURCE_DIR = 'static/_source/{{skin}}/'


def create_new_cloudportals_for_each_customization(logger):
    logger.stdout.write(logger.style.SUCCESS("\nCreating cloud portal for each customization"))
    customizations = Customization.objects.all()

    for customization in customizations:
        records_with_name = DataRecord.objects.filter(data_structure__name="%PRODUCT_NAME%",
                                                      customization=customization) \
            .exclude(version=None)
        if records_with_name.exists():
            product_name = records_with_name.latest('id').value
            logger.stdout.write(logger.style.SUCCESS("\tProduct name for {} is {}".\
                                                     format(customization.name, product_name)))
        else:
            product_name = "Nx Cloud"
            logger.stdout.write(logger.style.SUCCESS("\tCouldnt find product name for {} using {}".
                                                     format(customization.name, product_name)))
        cloud = structure.find_or_add_product(product_name, customization)
        cloud.customizations = [customization.id]
        cloud.save()
    logger.stdout.write(logger.style.SUCCESS("Done creating new cloud portals"))


def move_contexts_to_producttype(logger):
    logger.stdout.write(
        logger.style.SUCCESS("\nMoving contexts from original cloud portal to product_type cloud_portal"))
    cloud_portal = Product.objects.get(name="cloud_portal")
    cloud_portal_type = structure.find_or_add_product_type(ProductType.PRODUCT_TYPES.cloud_portal)

    for context in cloud_portal.context_set.all():
        logger.stdout.write(logger.style.SUCCESS("\tMoving {}".format(context.name)))
        context.product_type = cloud_portal_type
        context.save()
    logger.stdout.write(logger.style.SUCCESS("Done moving contexts to product_type cloud_portal"))


def move_revisions_to_new_cloud_portals(logger):
    logger.stdout.write(logger.style.SUCCESS("Moving revisions to new cloud portals"))
    original_cloud_portal = Product.objects.get(id=1)

    new_clouds = Product.objects.filter(product_type__type=ProductType.PRODUCT_TYPES.cloud_portal) \
        .exclude(id=original_cloud_portal.id)

    original_content_versions = ContentVersion.objects.filter(product=original_cloud_portal)

    for cloud in new_clouds:
        logger.stdout.write(
            logger.style.SUCCESS("\tMoving {} revisions to {}".\
                                 format(cloud.customizations.first(), cloud.name)))
        customization_content_versions = original_content_versions.filter(
            customization=cloud.customizations.first())
        for content_version in customization_content_versions:
            content_version.product = cloud
            content_version.save()
            for datarecord in content_version.datarecord_set.all():
                datarecord.product = cloud
                datarecord.save()
    logger.stdout.write(logger.style.SUCCESS("Done moving revisions to new cloud portals"))


def migrate_18_3_to_18_4(logger):
    if ProductType.objects.all().exists():
        logger.stdout.write(logger.style.SUCCESS("Migration has already been completed skipping this step"))
        return

    move_contexts_to_producttype(logger)
    create_new_cloudportals_for_each_customization(logger)
    move_revisions_to_new_cloud_portals(logger)

    logger.stdout.write(logger.style.SUCCESS("Done moving records from 18.3 to 18.4"))


def context_for_file(filename, skin_name):
    custom_dir = SOURCE_DIR.replace("{{skin}}", skin_name)
    context_name = filename.replace(custom_dir, '')
    match = re.search(r'lang_(.+?)/', context_name)
    language = None
    if match:
        language = match.group(1)
        context_name = context_name.replace(
            match.group(0), 'lang_{{language}}/')
    return context_name, language


def customizable_file(filename, ignore_not_english):
    supported_format = filename.endswith('.json') or \
        filename.endswith('.html') or \
        filename.endswith('.mustache') or \
        filename.endswith('apple-app-site-association')
    supported_directory = not ignore_not_english or \
        "lang_" not in filename or "lang_en_US" in filename
    return supported_format and supported_directory


def iterate_cms_files(skin_name, ignore_not_english):
    custom_dir = SOURCE_DIR.replace("{{skin}}", skin_name)
    for root, dirs, files in os.walk(custom_dir):
        for filename in files:
            file = os.path.join(root, filename)
            if customizable_file(file, ignore_not_english):
                yield file


def find_or_add_context_by_file(file_path, product_type, has_language):
    if Context.objects.filter(file_path=file_path, product_type=product_type).exists():
        return Context.objects.get(file_path=file_path, product_type=product_type)
    context = Context(name=file_path, file_path=file_path, product_type =product_type,
                      translatable=has_language, hidden=True, is_global=False)
    context.save()
    return context


def find_or_add_context_template(context, language_code, skin):
    if ContextTemplate.objects.filter(context__id=context.id, language__code=language_code, skin=skin).exists():
        return ContextTemplate.objects.get(context__id=context.id, language__code=language_code, skin=skin)
    context_template = ContextTemplate(context=context, language=Language.by_code(language_code), skin=skin)
    context_template.save()
    return context_template


def read_cms_strings(filename):
    pattern = re.compile(r'%\S+?%')
    with open(filename, 'r') as file:
        data = file.read()
        return data, set(re.findall(pattern, data))


def read_structure_file(filename, product_type, global_strings, skin):
    context_name, language_code = context_for_file(filename, skin)

    # now read file and get records from there.
    data, strings = read_cms_strings(filename)
    if not strings:  # if there is no records at all - we ignore it
        return

    # now, here this is customization-depending file

    # Here we check if there are any unique strings (which are not global)
    strings = [string for string in strings if string not in global_strings]
    context = find_or_add_context_by_file(context_name, product_type, bool(language_code))
    context_template = find_or_add_context_template(context, language_code, skin)
    context_template.template = data  # update template for this context
    context_template.save()
    for string in strings:
        structure.find_or_add_data_structure(string, None, context.id, bool(language_code))


def read_structure(product_type):
    product_type = structure.find_or_add_product_type(product_type)
    global_strings = DataStructure.objects.\
        filter(context__is_global=True, context__product_type=product_type).\
        values_list("name", flat=True)
    for skin in settings.SKINS:
        for file in iterate_cms_files(skin, False):
            read_structure_file(file, product_type, global_strings, skin)


def find_or_add_language(language_code):
    language = Language.by_code(language_code)
    if not language:
        language = Language(code=language_code, name=language_code)

    if language.code == language.name:  # name and code are the same - try to update name
        # try to read language.json for LANGUAGE_NAME
        language_json_path = os.path.join(SOURCE_DIR.replace("{{skin}}", settings.DEFAULT_SKIN),
                                          "static", "lang_" + language_code,
                                          "language.json")

        with codecs.open(language_json_path, 'r', 'utf-8') as file_descriptor:
            language_content = json.load(file_descriptor)
        language_name = language_content["language_name"]
        language.name = language_name
        language.save()

    return language


def read_languages(skin_name):
    languages_dir = os.path.join(SOURCE_DIR.replace("{{skin}}", skin_name), "static")
    languages = [dir.replace('lang_','') for dir in os.listdir(languages_dir) if dir.startswith('lang_')]
    for language_code in languages:
        find_or_add_language(language_code)


class Command(BaseCommand):
    help = 'Creates initial structure for CMS in ' \
           'the database (contexts, datastructure)'

    def add_arguments(self, parser):
        parser.add_argument('product_type', nargs='?', default='cloud_portal')

    @timer
    def handle(self, *args, **options):
        migrate_18_3_to_18_4(self)
        product_type = ProductType.get_type_by_name(options['product_type'])
        read_languages(settings.DEFAULT_SKIN)
        if not Customization.objects.filter(name=settings.CUSTOMIZATION).exists():
            default_customization = Customization(name=settings.CUSTOMIZATION,
                                                  default_language=Language.by_code('en_US'))
            default_customization.save()
            default_customization.languages = [Language.by_code('en_US')]
            default_customization.save()
        structure.read_structure_json('cms/cms_structure.json')
        read_structure(product_type)
        self.stdout.write(self.style.SUCCESS(
            'Successfully initiated data structure for CMS'))
