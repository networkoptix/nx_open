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


def file_for_context(context, customization_name, language_code):
    file_name = context.file_path
    if context.translatable:
        file_name = file_name.replace('lang_{{language}}', 'lang_'+language_code)
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


def process_context_structure(customization, context, content,
                              language, version_id, preview):
    for datastructure in context.datastructure_set.all():
        content_value = datastructure.find_actual_value(customization, language, version_id)
        # replace marker with value
        if datastructure.type != DataStructure.get_type('Image'):
            content = content.replace(datastructure.name, content_value)
        elif content_value:
            image_storage = os.path.join('static', customization.name)
            if preview:
                image_storage = os.path.join(image_storage, 'preview')
            
            file_name = datastructure.name
            if language:
                file_name = file_name.replace("{{language}}", language.code)

            convert_b64_image_to_png(content_value, file_name, image_storage)
    return content


def save_content(filename, content):
    make_dir(filename)
    with codecs.open(filename, "w", "utf-8") as file:
        file.write(content)


def process_context(context, context_path, language_code, customization, preview, version_id, global_contexts):
    if language_code:
        language = Language.objects.filter(code=language_code)
        if not language.exists():
            return
        language = customization.default_language

    content = ''
    if not context.is_global:
        source_file = file_for_context(context, customization.name, language_code)
        with open(source_file, 'r') as file:
            content = file.read()

    content = process_context_structure(customization, context, content, language, version_id, preview)

    if not context.is_global:  # if corrent context is global - do not apply other contexts
        for global_context in global_contexts.all():
            content = process_context_structure(
                customization, global_context, content, None, version_id, preview)

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
            version_id = versions.latest('accepted_date').id
        else:
            version_id = 0
            incremental = False  # no version - do full update using default values
        models.customization_cache(customization, force=True)

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
            changed_languages = changed_records.filter(context_id=context.id).values_list('language').distinct()
            if changed_languages.exists(language_id=customization.default_language_id):
                # if default language changes - it can affect all languages in the context
                changed_languages = customization.languages_list

        # update affected languages
        for language_code in changed_languages:
            process_context(context, context.file_path, language_code,
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
