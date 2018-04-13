from datetime import datetime

from notifications.api import send
from django.contrib.auth.models import Permission
from django.db.models import Q

from PIL import Image
import base64

from .filldata import fill_content
from api.models import Account
from cms.models import *


def get_context_and_language(request_data, context_id, language_id):
    context = Context.objects.get(id=context_id) if context_id else None
    language = Language.objects.get(id=language_id) if language_id else None

    if not context and 'context' in request_data and request_data['context']:
        context = Context.objects.get(id=request_data['context'])

    if not language and 'language' in request_data and request_data['language']:
        language = Language.objects.get(id=request_data['language'])

    return context, language


def accept_latest_draft(customization, user):
    unaccepted_version = ContentVersion.objects.filter(
        accepted_date=None, customization=customization)
    if not unaccepted_version.exists():
        return " is currently publishing or has already been published"
    unaccepted_version = unaccepted_version.latest('created_date')
    unaccepted_version.accepted_by = user
    unaccepted_version.accepted_date = datetime.now()
    unaccepted_version.save()
    return None


def notify_version_ready(customization, version_id, product_name, exclude_user):
    perm = Permission.objects.get(codename='publish_version')
    users = Account.objects.\
        filter(Q(groups__permissions=perm) | Q(user_permissions=perm)).\
        filter(customization=customization, subscribe=True).\
        exclude(pk=exclude_user.pk).\
        distinct()

    for user in users:
        send(user.email, "review_version",
             {'id': version_id, 'product': product_name},
             customization.name)


def save_unrevisioned_records(customization, language, data_structures,
                              request_data, request_files, user):
    upload_errors = []
    for data_structure in data_structures:
        data_structure_name = data_structure.name
        ds_language = language
        if not data_structure.translatable:
            ds_language = None
        records = data_structure.datarecord_set\
            .filter(customization=customization,
                    language=ds_language)

        new_record_value = ""
        # If the DataStructure is supposed to be an image convert to base64 and
        # error check
        if data_structure.get_type('Image') == data_structure.type:
            # If a file has been uploaded try to save it
            if data_structure_name in request_files:
                new_record_value, dimensions, invalid_file_type = handle_image_upload(
                    request_files[data_structure_name])

                if invalid_file_type:
                    upload_errors.append(
                        (data_structure_name,
                         'Invalid file type. Can only upload ".png"')
                    )
                    continue

                # Gets the meta_settings form the DataStructure to check if the sizes are valid
                # if the length is zero then there is no meta settings
                if len(data_structure.meta_settings):
                    size_errors = check_image_dimensions(
                        data_structure_name,
                        data_structure.meta_settings,
                        dimensions)

                    if size_errors:
                        upload_errors.extend(size_errors)
                        continue
            # If neither case do nothing for this record
            else:
                continue
        else:
            new_record_value = request_data[data_structure_name]

        if records.exists():
            if new_record_value == records.latest('created_date').value:
                continue
        else:
            if data_structure.default == new_record_value:
                continue

        record = DataRecord(data_structure=data_structure,
                            language=ds_language,
                            customization=customization,
                            value=new_record_value,
                            created_by=user)
        record.save()

    return upload_errors


def update_latest_record_version(records, new_version):
    record = records.latest('created_date')
    if not record.version:
        record.version = new_version
        record.save()


def update_records_to_version(contexts, customization, version):
    languages = Language.objects.all()
    for context in contexts:
        for data_structure in context.datastructure_set.all():
            all_records = data_structure.datarecord_set.filter(
                customization=customization)

            if data_structure.translatable:
                for language in languages:
                    records_for_language = all_records.filter(language=language)
                    # Now only the latest records that can be published will have its
                    # version altered
                    if records_for_language.exists():
                        update_latest_record_version(records_for_language, version)

            elif all_records.exists():
                update_latest_record_version(all_records, version)


def strip_version_from_records(version):
    records_to_strip = DataRecord.objects.filter(version=version)
    for record in records_to_strip:
        record.version = None
        record.save()


def remove_unused_records(customization):
    nullify_records = DataRecord.objects.filter(
        customization_id=customization.id, version_id=None)
    if nullify_records.exists():
        for record in nullify_records:
            record.delete()


def generate_preview(context=None):
    fill_content(customization_name=settings.CUSTOMIZATION)
    return context.url + "?preview" if context else "?preview"


def publish_latest_version(customization, user):
    publish_errors = accept_latest_draft(customization, user)
    if not publish_errors:
        fill_content(customization_name=settings.CUSTOMIZATION, preview=False)
    return publish_errors

def send_version_for_review(customization, language, data_structures,
                            product, request_data, request_files, user):
    old_versions = ContentVersion.objects.filter(accepted_date=None)

    if old_versions.exists():
        old_version = old_versions.latest('created_date')
        strip_version_from_records(old_version)
        old_version.delete()

    upload_errors = save_unrevisioned_records(
        customization, language, data_structures,
        request_data, request_files, user)

    version = ContentVersion(customization=customization,
                             name="N/A", created_by=user)
    version.save()

    update_records_to_version(Context.objects.filter(
        product=product), customization, version)

    notify_version_ready(customization, version.id, product.name, user)
    return upload_errors


def get_records_for_version(version):
    data_records = version.datarecord_set.all()\
        .order_by('data_structure__context__name', 'language__code')
    contexts = {}

    for record in data_records:
        context_name = record.data_structure.context.name
        if context_name in contexts:
            contexts[context_name].append(record)
        else:
            contexts[context_name] = [record]
    return contexts


def handle_image_upload(image):
    encoded_string = base64.b64encode(image.read())
    file_type = image.content_type

    if file_type != 'image/png':
        return None, None, True

    newImage = Image.open(image)
    width, height = newImage.size
    return encoded_string, {'width': width, 'height': height}, False


def check_image_dimensions(data_structure_name,
                           meta_dimensions, image_dimensions):
    size_error_msgs = []
    if not meta_dimensions:
        return size_error_msgs

    if 'height' in meta_dimensions and \
                    meta_dimensions['height'] != image_dimensions['height']:
        error_msg = "Image height must be equal to {}." \
                    "Uploaded image's height is {}."\
            .format(meta_dimensions['height'], image_dimensions['height'])
        size_error_msgs.append((data_structure_name, error_msg))

    if 'width' in meta_dimensions and \
                    meta_dimensions['width'] != image_dimensions['width']:
        error_msg = "Image width must be equal to {}." \
                    "Uploaded image's width is {}."\
            .format(meta_dimensions['width'], image_dimensions['width'])
        size_error_msgs.append((data_structure_name, error_msg))

    if 'height_le' in meta_dimensions and \
                    meta_dimensions['height_le'] < image_dimensions['height']:
        error_msg = "Image height must be equal to or less than {}." \
                    "Uploaded image's height is {}."\
            .format(meta_dimensions['height_le'], image_dimensions['height'])
        size_error_msgs.append((data_structure_name, error_msg))

    if 'width_le' in meta_dimensions and \
                    meta_dimensions['width_le'] < image_dimensions['width']:
        error_msg = "Image width must be equal to or less than {}." \
                    "Uploaded image's width is {}."\
            .format(meta_dimensions['width_le'], image_dimensions['width'])
        size_error_msgs.append((data_structure_name, error_msg))

    if 'height_ge' in meta_dimensions and \
                    meta_dimensions['height_ge'] > image_dimensions['height']:
        error_msg = "Image height must be equal to or more than {}." \
                    "Uploaded image's height is {}."\
            .format(meta_dimensions['height_ge'], image_dimensions['height'])
        size_error_msgs.append((data_structure_name, error_msg))

    if 'width_ge' in meta_dimensions and \
                    meta_dimensions['width_ge'] > image_dimensions['width']:
        error_msg = "Image width must be equal to or more than {}." \
                    "Uploaded image's width is {}." \
            .format(meta_dimensions['width_ge'], image_dimensions['width'])
        size_error_msgs.append((data_structure_name, error_msg))

    return size_error_msgs
