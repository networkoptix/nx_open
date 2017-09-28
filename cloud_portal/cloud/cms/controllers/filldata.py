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


def process_context_structure(customization, context, content,
                              language, version_id, preview):
    for datastructure in context.datastructure_set.order_by('order').all():
        content_value = datastructure.find_actual_value(customization, language, version_id)
        # replace marker with value
        if datastructure.type not in (DataStructure.DATA_TYPES.image, DataStructure.DATA_TYPES.file):
            content = content.replace(datastructure.name, content_value)
        elif content_value:
            image_storage = os.path.join('static', customization.name)
            if preview:
                image_storage = os.path.join(image_storage, 'preview')
            
            file_name = datastructure.name
            if language:
                file_name = file_name.replace("{{language}}", language.code)

            save_b64_to_file(content_value, file_name, image_storage)
    return content


def save_content(filename, content):
    make_dir(filename)
    with codecs.open(filename, "w", "utf-8") as file:
        file.write(content)


def process_context(context, language_code, customization, preview, version_id, global_contexts):
    language = None
    if language_code:
        language = Language.objects.filter(code=language_code)
        if not language.exists():
            return
        language = customization.default_language

    content = process_context_structure(customization, context, context.template, language, version_id, preview)
    if not context.is_global:  # if current context is global - do not apply other contexts
        for global_context in global_contexts.all():
            content = process_context_structure(
                customization, global_context, content, None, version_id, preview)

    return content


def read_customized_file(filename, customization_name, language_code=None, version_id=None, preview=False):
    # 1. try to find context for this file
    customization = Customization.objects.get(name=customization_name)
    clean_name = filename.replace(language_code, "{{language}}") if language_code else filename
    context = Context.objects.filter(file_path=clean_name)
    if context.exists():
        # success -> return process_context
        context = context.first()
        global_contexts = Context.objects.filter(is_global=True, product_id=context.product_id)
        return process_context(context, language_code, customization, preview, version_id, global_contexts)

    # 2. try to find datastructure for this file
    # TODO: name is not unique
    data_structure = DataStructure.objects.filter(name=clean_name)
    if data_structure.exists():
        # success -> return actual value
        data_structure = data_structure.first()
        value = data_structure.find_actual_value(customization,
                                                 Language.objects.get(code=language_code) if language_code else None,
                                                 version_id)
        return base64.b64decode(value)

    # fail - try to read file from drive
    filename = filename.replace("{{language}}", language_code)
    file_path = os.path.join(settings.STATIC_LOCATION, customization.name, filename)
    try:  # try to read file as text
        with codecs.open(filename, 'r', 'utf-8') as file:
            return file.read()
    except IOError:
        pass

    try:  # try to read binary file
        with open(file_path, "rb") as file:
            return file.read()
    except IOError:
        return None  # nothing helps


def save_context(context, context_path, language_code, customization, preview, version_id, global_contexts):
    content = process_context(context, language_code, customization, preview, version_id, global_contexts)
    target_file_name = target_file(context_path, customization, language_code, preview)
    save_content(target_file_name, content)


def generate_languages_json(customization, preview):
    languages_json = [{"name": lang.name, "language": lang.code}
                      for lang in customization.languages.all()]
    target_file_name = target_file('static/languages.json', customization, None, preview)
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
                customization.change_preview_status(Customization.PREVIEW_STATUS.review)
            else:
                if incremental:
                    return  # When previewing awaiting version and state is review - do nothing
                pass
        else:  # draft
            if customization.preview_status != Customization.PREVIEW_STATUS.review:
                # When saving draft and state is review - do incremental update
                # applying all drafted changes and change state to draft
                # incremental = True
                customization.change_preview_status(Customization.PREVIEW_STATUS.draft)
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
            version_id = versions.latest('accepted_date').id
        else:
            version_id = 0
            incremental = False  # no version - do full update using default values
        customization_cache(customization, force=True)

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

        changed_context_ids = list(changed_records.values_list('data_structure__context_id', flat=True).distinct())
        changed_contexts = Context.objects.filter(id__in=changed_context_ids)

        changed_global_contexts = changed_contexts.filter(is_global = True)
        if changed_global_contexts.exists():  # global context was changed - force full rebuild
            incremental = False
        changed_contexts = changed_contexts.all()

    if changed_context:  # if we want to update only fixed content
        if changed_context.is_global:
            incremental = False
        else:
            changed_contexts = [changed_context]

    if not incremental:  # If not incremental - iterate all contexts and all languages
        changed_contexts = Context.objects.filter(product_id=product_id).all()
        changed_languages = customization.languages_list

    for context in changed_contexts:
        # now we need to check what languages were changes
        # if the default language is changed - we update all languages (lazy way)
        # otherwise - update only affected languages
        if incremental:
            changed_languages = changed_records.filter(data_structure__context_id=context.id).values_list('language').distinct()
            if changed_languages.filter(id=customization.default_language_id).exists():
                # if default language changes - it can affect all languages in the context
                changed_languages = customization.languages_list

        # update affected languages
        for language_code in changed_languages:
            save_context(context, context.file_path, language_code, customization, preview, version_id, global_contexts)

    generate_languages_json(customization, preview)


def save_b64_to_file(value, filename, storage_location):
    if not value:
        return
    file_name = os.path.join(storage_location, filename)
    make_dir(file_name)
    image_png = base64.b64decode(value)

    with open(file_name, 'wb') as f:
        f.write(image_png)
