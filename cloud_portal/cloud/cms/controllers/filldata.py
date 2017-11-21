from ..models import *
import os
import re
import json
import codecs
import base64


STATIC_DIR = 'static/{{customization}}/source/'


def make_dir(filename):
    dirname = os.path.dirname(filename)
    if not os.path.exists(dirname):
        try:
            os.makedirs(dirname)
        except OSError as exc:  # Guard against race condition
            if exc.errno != errno.EEXIST:
                raise


def customizable_file(filename, ignore_not_english):
    supported_format = filename.endswith('.json') or \
        filename.endswith('.html') or \
        filename.endswith('.mustache') or \
        filename.endswith('apple-app-site-association')
    supported_directory = not ignore_not_english or \
        "lang_" not in filename or "lang_en_US" in filename
    return supported_format and supported_directory


def context_for_file(filename, customization_name):
    custom_dir = STATIC_DIR.replace("{{customization}}", customization_name)
    context_name = filename.replace(custom_dir, '')
    match = re.search(r'lang_(.+?)/', context_name)
    language = None
    if match:
        language = match.group(1)
        context_name = context_name.replace(
            match.group(0), 'lang_{{language}}/')
    return context_name, language


def iterate_cms_files(customization_name, ignore_not_english):
    custom_dir = STATIC_DIR.replace("{{customization}}", customization_name)
    for root, dirs, files in os.walk(custom_dir):
        for filename in files:
            file = os.path.join(root, filename)
            if customizable_file(file, ignore_not_english):
                yield file


def process_context_structure(customization, context, content,
                              language, version_id, preview):
    for record in context.datastructure_set.all():
        content_record = None
        content_value = None
        # try to get translated content
        if language and record.translatable:
            content_record = DataRecord.objects\
                .filter(language_id=language.id,
                        data_structure_id=record.id,
                        customization_id=customization.id)

        # if not - get record without language
        if not content_record or not content_record.exists():
            content_record = DataRecord.objects\
                .filter(language_id=None,
                        data_structure_id=record.id,
                        customization_id=customization.id)

        # if not - get default language
        if not content_record or not content_record.exists():
            content_record = DataRecord.objects\
                .filter(language_id=customization.default_language_id,
                        data_structure_id=record.id,
                        customization_id=customization.id)

        if content_record and content_record.exists():
            if not version_id:
                content_value = content_record.latest('created_date').value
            else:  # Here find a datarecord with version_id
                   # which is not more than version_id
                   # filter only accepted content_records
                content_record = content_record.filter(
                    version_id__lte=version_id)
                if content_record.exists():
                    content_value = content_record.latest('version_id').value

        if not content_value:  # if no value - use default value from structure
            content_value = record.default

        # replace marker with value
        if record.type != DataStructure.get_type('Image'):
            content = content.replace(record.name, content_value)
        elif content_record.exists():
            image_storage = os.path.join('static', customization.name)
            if preview:
                image_storage = os.path.join(image_storage, 'preview')
            
            file_name = record.name
            if language:
                file_name = file_name.replace("{{language}}", language.code)

            convert_b64_image_to_png(content_value, file_name, image_storage)
    return content


def process_file(source_file, customization, product_id, preview, version_id):
    context_name, language_code = context_for_file(
        source_file, customization.name)

    branding_context = Context.objects.filter(name='branding')
    context = Context.objects.filter(
        file_path=context_name, product_id=product_id)
    language = None
    if language_code:
        language = Language.objects.filter(code=language_code)
        if not language.exists():
            return
        language = language.first()

    with open(source_file, 'r') as file:
        content = file.read()

    if context.exists():
        content = process_context_structure(
            customization, context.first(), content, language, version_id, preview)
    if branding_context.exists():
        content = process_context_structure(
            customization, branding_context.first(), content, None, version_id, preview)

    filename = context_name
    if language_code:
        filename = filename.replace("{{language}}", language_code)
    # write content to target place
    if not preview:
        target_file = os.path.join('static', customization.name, filename)
    else:
        target_file = os.path.join(
            'static', customization.name, 'preview', filename)
    make_dir(target_file)
    with open(target_file, 'w') as file:
        file.write(content)


def generate_languages_json(customization_name, preview):
    def save_content(filename, content):
        if filename:
            # proceed with branding
            make_dir(filename)
            with codecs.open(filename, "w", "utf-8") as file:
                file.write(content)
    customization = Customization.objects.get(name=customization_name)
    languages_json = [{"name": lang.name, "language": lang.code}
                      for lang in customization.languages.all()]
    if not preview:
        target_file = os.path.join(
            'static', customization.name, 'static', 'languages.json')
    else:
        target_file = os.path.join(
            'static', customization.name, 'preview',
            'static', 'languages.json')
    save_content(target_file, json.dumps(languages_json, ensure_ascii=False))


def fill_content(customization_name='default', product='cloud_portal',
                 preview=True, version_id=None):

    # if preview=False
    #   retrieve latest accepted version
    #   if version_id is not None and version_id!=latest_id - raise exception
    # else
    #   if version_id is None - preview latest available datarecords
    #   else - preview specific version
    product_id = Product.objects.get(name=product).id
    customization = Customization.objects.get(name=customization_name)

    if not preview:
        if version_id is not None:
            raise Exception(
                'Only latest accepted version can be published\
                 without preview flag, version_id id forbidden')
        versions = ContentVersion.objects.filter(
            customization_id=customization.id, accepted_date__isnull=False)
        if versions.exists():
            version_id = versions.latest('accepted_date')
        else:
            version_id = 0

    # iterate all files (same way we fill structure)
    for source_file in iterate_cms_files(customization_name, False):
        process_file(source_file, customization,
                     product_id, preview, version_id)

    generate_languages_json(customization_name, preview)


def convert_b64_image_to_png(value, filename, storage_location):
    if not value:
        return

    file_name = os.path.join(storage_location, filename)
    make_dir(file_name)
    image_png = base64.b64decode(value)

    with open(file_name, 'wb') as f:
        f.write(image_png)
