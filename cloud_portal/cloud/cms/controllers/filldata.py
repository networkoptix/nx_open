from ..models import *
import os
import json
import codecs
import base64
import zipfile
import distutils.dir_util
import errno
import traceback
from StringIO import StringIO

from cloud.debug import timer

import logging
logger = logging.getLogger(__name__)

SOURCE_DIR = 'static/_source/{{skin}}'
TARGET_DIR = 'static/{{customization}}'


def make_dir(filename):
    dirname = os.path.dirname(filename)
    if not os.path.exists(dirname):
        try:
            os.makedirs(dirname)
        except OSError as exc:  # Guard against race condition
            if exc.errno != errno.EEXIST:
                raise


def target_file(file_name, save_location, language_code, preview):
    if language_code:
        file_name = file_name.replace("{{language}}", language_code)
    # write content to target place
    if not preview:
        target_file_name = os.path.join('static', save_location, file_name)
    else:
        target_file_name = os.path.join(
            'static', save_location, 'preview', file_name)
    return target_file_name


def process_context_structure(product, context, content, language,
                              version_id, preview, force_global_files):

    def replace_in(adict, key, value):
        for dict_key in adict.keys():
            itm_type = type(adict[dict_key])
            if itm_type not in [str, unicode, dict, list]:
                continue

            if itm_type is list:
                for item in adict[dict_key]:
                    if type(item) in [str, unicode]:
                        idx = adict[dict_key].index(item)
                        adict[dict_key][idx] = item.replace(key, value)
                    elif item in [dict, list]:
                        replace_in(item, key, value)

            elif itm_type is dict:
                replace_in(adict[dict_key], key, value)

            elif key in adict[dict_key]:
                adict[dict_key] = adict[dict_key].replace(key, value)

    default_language = product.default_language
    location = product.product_root
    for datastructure in context.datastructure_set.order_by('order').all():
        try:
            content_value = datastructure.find_actual_value(product, language, version_id)
            # replace marker with value
            if datastructure.type not in (DataStructure.DATA_TYPES.image, DataStructure.DATA_TYPES.file):
                if type(content) == dict:
                    # Process language JSON file
                    replace_in(content, datastructure.name, content_value)
                else:
                    if datastructure.type == DataStructure.DATA_TYPES.check_box:
                        content_value = str(content_value)
                    content = content.replace(datastructure.name, content_value)

            elif content_value or datastructure.optional:
                if context.is_global and not force_global_files:
                    # do not update files from global contexts all the time
                    continue

                if not datastructure.translatable and language != default_language:
                    # if file itself is not translatable - update it only for default language
                    continue

                image_storage = os.path.join('static', location)
                if preview:
                    image_storage = os.path.join(image_storage, 'preview')

                file_name = datastructure.name
                if language:
                    file_name = file_name.replace("{{language}}", language.code)

                # print "Save file from DB: " + file_name, context, language, context.is_global
                save_b64_to_file(content_value, file_name, image_storage)
        except Exception:
            # if something happens here - instance will not start and it will close to impossible to fix
            # so we ignore broken records while logging them - it will raise cloud alarm and we will go and fix the problem
            logger.error("ERROR: Cannot process data structure {0} for product {1}".format(
                datastructure.name, product.name))
            logger.error(traceback.format_exc())

    return content


def save_content(filename, content):
    make_dir(filename)
    with codecs.open(filename, "w", "utf-8") as file:
        file.write(content)


def process_context(product, context, language_code,
                    preview, version_id, global_contexts):
    default_language = product.default_language
    language = Language.by_code(language_code, default_language)
    skin = product.read_global_value('%SKIN%')
    context_template_text = context.template_for_language(language, default_language, skin)

    # check if the file is language JSON
    if context.file_path.endswith(".json") and isinstance(context_template_text, unicode):
        try:
            context_template_text = json.loads(context_template_text)
        except ValueError:
            print("Failed to decode file -> " + context.file_path)

    if not context_template_text:
        context_template_text = ''
    content = process_context_structure(product, context, context_template_text,
                                        language, version_id, preview, context.is_global)  # if context is global - process it
    if not context.is_global:  # if current context is global - do not apply other contexts
        for global_context in global_contexts.all():
            content = process_context_structure(product, global_context, content, None, version_id, preview, False)

    # If json -> dump it to string
    if type(content) == dict:
        content = json.dumps(content)

    return content


def read_customized_file(filename, product, language_code=None,
                         version_id=None, preview=False):
    # 1. try to find context for this file

    clean_name = filename.replace(language_code, "{{language}}") if language_code else filename
    context = Context.objects.filter(file_path=clean_name, product_type=product.product_type)
    if context.exists():
        # success -> return process_context
        context = context.first()
        global_contexts = Context.objects.filter(is_global=True, product_type=product.product_type)
        return process_context(product, context, language_code, preview, version_id, global_contexts)

    # 2. try to find datastructure for this file
    # TODO: name is not unique
    data_structure = DataStructure.objects.filter(name=clean_name)
    if data_structure.exists():
        # success -> return actual value
        data_structure = data_structure.first()
        value = data_structure.find_actual_value(product, Language.by_code(language_code), version_id)
        return base64.b64decode(value)

    # fail - try to read file from drive
    filename = filename.replace("{{language}}", language_code)
    file_path = os.path.join(settings.STATIC_LOCATION, product.product_root, filename)
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


def save_context(product, context, context_path, language_code,
                 preview, version_id, global_contexts):
    content = process_context(product, context, language_code,
                              preview, version_id, global_contexts)

    default_language = product.default_language
    language = Language.by_code(language_code, default_language)
    skin = product.read_global_value('%SKIN%')
    if context.template_for_language(language, default_language, skin):  # if we have template - save context to file
        target_file_name = target_file(context_path, product.product_root, language_code, preview)
        # print "save file: " + target_file_name
        save_content(target_file_name, content)


def generate_languages_json(save_location, language_codes, preview):
    languages = Language.objects.filter(code__in=language_codes)
    languages_json = [{"name": lang.name, "language": lang.code} for lang in languages]
    target_file_name = target_file('static/languages.json', save_location, None, preview)
    save_content(target_file_name, json.dumps(languages_json, ensure_ascii=False))


def init_skin(product, preview=False):
    # 1. read skin for this customization
    customization_name = product.customizations.first().name
    skin = product.read_global_value('%SKIN%')
    logger.info("Init " + skin + " skin for " + product.__str__())

    # 2. copy directory
    from_dir = SOURCE_DIR.replace("{{skin}}", skin)
    target_dir = TARGET_DIR.replace("{{customization}}", customization_name)

    # 3. run fill_content
    if not preview:
        distutils.dir_util.copy_tree(from_dir, target_dir)
        logger.info("Fill content for " + product.__str__())
        fill_content(product, preview=False, incremental=False)
    else:
        distutils.dir_util.copy_tree(from_dir, os.path.join(target_dir, 'preview'))
        logger.info("Fill preview for " + product.__str__())
        fill_content(product, preview=True, incremental=False)


@timer
def fill_content(product,
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
    if not product.product_type.can_preview:
        return

    if preview:  # Here we decide, if we need to change preview state
        # if incremental was false initially - we keep it as false
        if version_id:
            if product.preview_status != Product.PREVIEW_STATUS.review:
                # When previewing awaiting version and state is draft
                # if we are just sending version to review - do incremental update
                if not send_to_review:
                    incremental = False  # otherwise - do full update and change state to review
                product.change_preview_status(Product.PREVIEW_STATUS.review)
            else:
                if incremental:
                    return  # When previewing awaiting version and state is review - do nothing
                pass
        else:  # draft
            if product.preview_status == Product.PREVIEW_STATUS.review:
                # When saving draft and state is review - do incremental update
                # applying all drafted changes and change state to draft
                # incremental = True
                product.change_preview_status(Product.PREVIEW_STATUS.draft)
                changed_context = None  # remove changed context so that we do full incremental update
            else:
                # When saving draft for context and state is draft - do incremental update only for changed context
                # update only changed context
                # keep incremental value
                pass

    global_contexts = Context.objects.filter(is_global=True, product_type=product.product_type)

    if not preview:
        if version_id is not None:
            raise Exception(
                'Only latest accepted version can be published\
                 without preview flag, version_id id forbidden')
        versions = ContentVersion.objects.filter(
            product_id=product.id, accepted_date__isnull=False)
        if versions.exists():
            version_id = versions.latest('accepted_date').id
        else:
            version_id = 0
            incremental = False  # no version - do full update using default values
        if product.product_type.type == ProductType.PRODUCT_TYPES.cloud_portal:
            cloud_portal_customization_cache(product.product_root, force=True)

    if incremental and not changed_context:
        # filter records changed in this version
        # get their datastructures
        # detect their contexts

        changed_records = DataRecord.objects.filter(version_id=version_id, product=product)
        # in case version_id is none - we need to filter by product as well
        if not version_id:  # if version_id is None - check if records are actually latest
            changed_records_ids = [DataRecord.objects.
                                   filter(language_id=record.language_id,
                                          data_structure_id=record.data_structure_id,
                                          product=product).
                                   latest('created_date').id for record in changed_records]
            changed_records = changed_records.filter(id__in=changed_records_ids)

        changed_context_ids = list(changed_records.values_list('data_structure__context_id', flat=True).distinct())
        changed_contexts = Context.objects.filter(id__in=changed_context_ids)

        changed_global_contexts = changed_contexts.filter(is_global=True)
        if changed_global_contexts.exists():  # global context was changed - force full rebuild
            incremental = False
        changed_contexts = changed_contexts.all()

    if changed_context:  # if we want to update only fixed context
        if changed_context.is_global:
            incremental = False
        else:
            changed_contexts = [changed_context]
            changed_records = DataRecord.objects.filter(data_structure__context=changed_context,
                                                        version_id=version_id,
                                                        product=product)
            if not version_id:
                changed_records_ids = [DataRecord.objects.
                                       filter(language_id=record.language_id,
                                              data_structure_id=record.data_structure.id,
                                              product=product).
                                       latest('created_date').id for record in changed_records]
                changed_records = changed_records.filter(id__in=changed_records_ids)

    if not incremental:  # If not incremental - iterate all contexts and all languages
        changed_contexts = Context.objects.filter(product_type=product.product_type).all()
        changed_languages = product.languages_list

    default_language_code = product.default_language.code
    languages_list = product.languages_list
    for context in changed_contexts:
        #logger.info("Process context: " + context.name + " file:" + context.file_path)
        # now we need to check what languages were changes
        # if the default language is changed - we update all languages (lazy way)
        # otherwise - update only affected languages
        if incremental:
            changed_languages = list(changed_records.filter(data_structure__context_id=context.id).\
                values_list('language__code', flat=True).distinct())

            if default_language_code in changed_languages:
                # if default language changes - it can affect all languages in the context
                changed_languages = languages_list

        # update affected languages
        if context.translatable:
            for language_code in changed_languages:
                save_context(product, context, context.file_path, language_code, preview, version_id, global_contexts)
        else:
            save_context(product, context, context.file_path, None, preview, version_id, global_contexts)

    generate_languages_json(product.product_root, languages_list,  preview)


def zip_context(zip_file, product, context, language_code,
                preview, version_id, global_contexts, add_root):
    default_language = product.default_language
    language = Language.by_code(language_code, default_language)
    root_dir = product.product_root
    skin = product.read_global_value('%SKIN%')
    if context.template_for_language(language, default_language, skin):  # if we have template - save context to file
        data = process_context(product, context, language_code, preview, version_id, global_contexts)
        name = context.file_path.replace("{{language}}", language_code) if language_code else context.file_path
        if add_root:
            name = os.path.join(root_dir, name)
        zip_file.writestr(name, data)
    file_structures = context.datastructure_set.filter(type__in=(DataStructure.DATA_TYPES.image,
                                                                 DataStructure.DATA_TYPES.file))
    for file_structure in file_structures:
        data = file_structure.find_actual_value(product, Language.by_code(language_code), version_id)
        data = base64.b64decode(data)
        name = file_structure.name.replace("{{language}}", language_code) if language_code else file_structure.name
        if add_root:
            name = os.path.join(root_dir, name)
        zip_file.writestr(name, data)


def get_zip_package(product, preview=True, version_id=None, add_root=True):
    zip_data = StringIO()
    zip_file = zipfile.ZipFile(zip_data, "a", zipfile.ZIP_DEFLATED, False)

    global_contexts = Context.objects.filter(is_global=True, product_type=product.product_type)
    languages = product.languages_list

    for context in product.product_type.context_set.all():
        if context.translatable:
            for language_code in languages:
                zip_context(zip_file, product, context, language_code,
                            preview, version_id, global_contexts, add_root)
        else:
            zip_context(zip_file, product, context, None,
                        preview, version_id, global_contexts, add_root)

    # Mark the files as having been created on Windows so that
    # Unix permissions are not inferred as 0000
    for file in zip_file.filelist:
        file.create_system = 0

    zip_file.close()
    zip_data.seek(0)
    return zip_data.read()


def save_b64_to_file(value, filename, storage_location):
    file_name = os.path.join(storage_location, filename)
    make_dir(file_name)

    image_png = base64.b64decode(value) if value else ''

    with open(file_name, 'wb') as f:
        f.write(image_png)
