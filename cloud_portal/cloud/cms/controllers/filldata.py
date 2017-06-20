from ..models import *
import os
import re


STATIC_DIR = 'static/{{customization}}/source/'


def make_dir(filename):
    dirname = os.path.dirname(filename)
    if not os.path.exists(dirname):
        try:
            os.makedirs(dirname)
        except OSError as exc:  # Guard against race condition
            if exc.errno != errno.EEXIST:
                raise


def customizable_file(filename):
    supported_format = filename.endswith('.json') or filename.endswith('.html') or filename.endswith('.mustache')
    supported_directory = "lang_" not in filename or "lang_en_US" in filename
    return supported_format and supported_directory


def context_for_file(filename, customization_name):
    custom_dir = STATIC_DIR.replace("{{customization}}", customization_name)
    context_name = filename.replace(custom_dir, '')
    match = re.search(r'lang_(.+?)/', context_name)
    language = None
    if match:
        language = match.group(1)
        context_name = context_name.replace(match.group(0), 'lang_{{language}}/')
    return context_name, language


def iterate_cms_files(customization_name):
    custom_dir = STATIC_DIR.replace("{{customization}}", customization_name)
    for root, dirs, files in os.walk(custom_dir):
        for filename in files:
            file = os.path.join(root, filename)
            if customizable_file(file):
                yield file


def process_context_structure(customization, context, content, language):
    for record in context.datastructure_set.all():
        # try to get translated content
        if language:
            content_record = DataRecord.objects.filter(language_id=language.id,
                                                       data_structure_id=record.id,
                                                       customization_id=customization.id)
        # if not - get default language
        if not content_record.exists():
            content_record = DataRecord.objects.filter(language_id=customization.default_language_id,
                                                       data_structure_id=record.id,
                                                       customization_id=customization.id)

        # if not - get record without language
        if not content_record.exists():
            content_record = DataRecord.objects.filter(language_id=None,
                                                       data_structure_id=record.id,
                                                       customization_id=customization.id)

        if content_record and content_record.exists():
            content_value = content_record.first().value
        else:  # if no value - use default value from structure
            content_value = record.default

        # replace marker with value
        content = content.replace(record.name, content_value)
    return content


def process_file(source_file, customization, product_id, preview):
    context_name, language_code = context_for_file(source_file, customization.name)

    branding_context = Context.objects.filter(name='branding')
    context = Context.objects.filter(name=context_name, product_id=product_id)
    if language_code:
        language = Language.objects.get(code=language_code)

    with open(source_file, 'r') as file:
        content = file.read()

    if branding_context.exists():
        content = process_context_structure(customization, branding_context.first(), content, None)
    if context.exists() and language:
        content = process_context_structure(customization, context.first(), content, language)

    filename = context_name
    if language_code:
        filename = filename.replace("{{language}}", language_code)
    # write content to target place
    if not preview:
        target_file = os.path.join('static', customization.name, filename)
    else:
        target_file = os.path.join('static', customization.name, 'preview', filename)
    make_dir(target_file)
    with open(target_file, 'w') as file:
        file.write(content)


def fill_content(customization_name='default', product='cloud_portal', preview=True):
    product_id = Product.objects.get(name=product).id
    customization = Customization.objects.get(name=customization_name)

    # iterate all files (same way we fill structure)
    for source_file in iterate_cms_files(customization_name):
        process_file(source_file, customization, product_id, preview)


# from cms.controllers.filldata import *
