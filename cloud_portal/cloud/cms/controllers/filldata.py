from ..models import *
import os
import re
import json
import codecs
import base64


SOURCE_DIR = 'static/{{customization}}/source/'


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
    custom_dir = SOURCE_DIR.replace("{{customization}}", customization_name)
    context_name = filename.replace(custom_dir, '')
    match = re.search(r'lang_(.+?)/', context_name)
    language = None
    if match:
        language = match.group(1)
        context_name = context_name.replace(
            match.group(0), 'lang_{{language}}/')
    return context_name, language


def file_for_context(context, language, customization_name):
    file_name = context.name
    if context.translatable:
        file_name = file_name.replace('lang_{{language}}', 'lang_'+language)
    custom_dir = SOURCE_DIR.replace("{{customization}}", customization_name)
    return os.path.join(custom_dir, file_name)


def target_file(file_name, customization, language_code, preview):
    if language_code:
        file_name = file_name.replace("{{language}}", language_code)
    # write content to target place
    if not preview:
        target_file_name = os.path.join('static', customization.name, file_name)
    else:
        target_file_name = os.path.join(
            'static', customization.name, 'preview', file_name)
    return target_file_name


def iterate_cms_files(customization_name, ignore_not_english):
    custom_dir = SOURCE_DIR.replace("{{customization}}", customization_name)
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


def save_content(filename, content):
    make_dir(filename)
    with codecs.open(filename, "w", "utf-8") as file:
        file.write(content)


def process_context_for_file(source_file, context, context_path, language_code,
                             customization, preview,
                             version_id, global_contexts):

    if language_code:
        language = Language.objects.filter(code=language_code)
        if not language.exists():
            return
        language = language.first()

    with open(source_file, 'r') as file:
        content = file.read()

    if context.exists() and language:
        content = process_context_structure(
            customization, context.first(), content, language, version_id, preview)

    for global_context in global_contexts.all():
        content = process_context_structure(
            customization, global_context, content, None, version_id, preview)

    target_file_name = target_file(context_path, customization, language_code, preview)
    save_content(target_file_name, content)


def generate_languages_json(customization, preview):
    languages_json = [{"name": lang.name, "language": lang.code}
                      for lang in customization.languages.all()]
    target_file_name = target_file('languages.json', customization, None, preview)
    save_content(target_file_name, json.dumps(languages_json, ensure_ascii=False))


def fill_content(customization_name='default', product='cloud_portal',
                 preview=True,
                 version_id=None,
                 incremental=False,
                 changed_context=None,
                 send_to_review=False):

    # if preview=False
    #   retrieve latest accepted version
    #   if version_id is not None and version_id!=latest_id - raise exception
    # else
    #   if version_id is None - preview latest available datarecords
    #   else - preview specific version
    product_id = Product.objects.get(name=product).id
    customization = Customization.objects.get(name=customization_name)

    if preview:  # Here we decide, if we need to change preview state
        # if incremental was false initially - we keep it as false
        if version_id:
            if customization.preview_status != Customization.PREVIEW_STATUS.review:
                # When previewing awaiting version and state is draft
                # if we are just sending version to review - do incremental update
                if not send_to_review:
                    incremental = False  # otherwise - do full update and change state to review
                customization.preview_status = Customization.PREVIEW_STATUS.review
                customization.save()
            else:
                if incremental:
                    return  # When previewing awaiting version and state is review - do nothing
                pass
        else:  # draft
            if customization.preview_status != Customization.PREVIEW_STATUS.review:
                # When saving draft and state is review - do incremental update
                # applying all drafted changes and change state to draft
                # incremental = True
                customization.preview_status = Customization.PREVIEW_STATUS.draft
                customization.save()
                changed_context = None  # remove changed context so that we do full incremental update
            else:
                # When saving draft for context and state is draft - do incremental update only for changed context
                # update only changed context
                # keep incremental value
                pass

    global_contexts = Context.objects.filter(is_global=True, product_id=product_id)

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
            incremental = False  # no version - do full update using default values

    if incremental and not changed_context:
        # filter records changed in this version
        # get their datastructures
        # detect their contexts

        changed_records = DataRecord.objects.filter(version_id=version_id)
        if not version_id:  # if version_id is None - check if records are actually latest
            changed_records = [record for record in changed_records if
                               record.id == DataRecord.objects.
                               filter(language_id=record.language_id,
                                      data_structure_id=record.data_structure_id,
                                      customization_id=customization.id).
                               latest('created_date').id]

        changed_contexts = not changed_records. \
            values_list('data_structure', flat=True). \
            values_list('context', flat=True). \
            distinct()
        changed_global_contexts = changed_contexts.filter(is_global = True)
        if changed_global_contexts.exists():  # global context was changed - force full rebuild
            incremental = False
        changed_contexts = changed_contexts.all()

    if changed_context:  # if we want to update only fixed content
        if changed_context.is_global:
            incremental = False
        else:
            changed_contexts = [changed_context]

    if not incremental:
        # iterate all files (same way we fill structure)
        for source_file in iterate_cms_files(customization_name, False):
            context_path, language_code = context_for_file(source_file, customization.name)
            context = Context.objects.filter(file_path=context_path)
            process_context_for_file(source_file, context, context_path, language_code,
                                     customization, preview,
                                     version_id, global_contexts)
    else:
        for context in changed_contexts:
            # now we need to check what languages were changes
            # if the default language is changed - we update all languages (lazy way)
            # otherwise - update only affected languages
            changed_languages = changed_records.values_list('language').distinct()
            if changed_languages.exists(language_id=customization.default_language_id):
                # update all languages in the context
                changed_languages = customization.languages.values_list('code', flat=True)

            # update affected languages
            for language in changed_languages:
                source_file = file_for_context(context, customization, language.code)
                process_context_for_file(source_file, context, context.path, language.code,
                                         customization, preview, version_id, global_contexts)

    generate_languages_json(customization, preview)


def convert_b64_image_to_png(value, filename, storage_location):
    if not value:
        return

    file_name = os.path.join(storage_location, filename)
    make_dir(file_name)
    image_png = base64.b64decode(value)

    with open(file_name, 'wb') as f:
        f.write(image_png)
