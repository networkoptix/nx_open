from datetime import datetime

from notifications.notifications_api import send
from django.contrib.auth.models import Permission
from django.db.models import Q

from PIL import Image
import base64
import json
import re
import uuid
import hashlib
import yaml

from .filldata import fill_content
from api.models import Account
from ..models import *

BYTES_TO_MEGABYTES = 1048576.0


def update_draft_state(review_id, target_state, user):
    review = ProductCustomizationReview.objects.filter(id=review_id, reviewed_by=None)
    if not review.exists():
        return " is currently publishing or has already been published"

    review = review.latest('id')

    if not review.version.accepted_by:
        review.version.accepted_by = user
        review.version.accepted_date = datetime.now()
        review.version.save()

    review.update_state(user, target_state)

    return None


def notify_version_ready(product, version_id, exclude_user):
    perm = Permission.objects.filter(codename='publish_version')
    users = Account.objects.\
        filter(Q(groups__permissions=perm) | Q(user_permissions=perm)).\
        filter(subscribe=True, customization__in=product.customizations.all()).\
        exclude(pk=exclude_user.pk).\
        distinct()

    product_name = product.name

    for user in users:
        send(user.email, "review_version",
             {'id': version_id, 'product': product_name},
             user.customization)


def save_unrevisioned_records(product, context, language, data_structures,
                              request_data, request_files, user, version_id=None):
    upload_errors = []
    for data_structure in data_structures:
        data_structure_name = data_structure.name
        ds_language = language
        if not data_structure.translatable:
            ds_language = None

        new_record_value = ""
        external_file = None
        delete_file = False
        latest_value = data_structure.find_actual_value(product, ds_language)
        # If the DataStructure is supposed to be an image convert to base64 and
        # error check
        # TODO: Refactor image/file logic - CLOUD-1524
        """
            Currently if the data structure is optional you can remove the value.
            
            Planned change is to make it to where you can "delete" the value and if its not optional then fallback
            to the default value.
            
            This will create a new record making images/files behave like the other data structure types
            Places to touch are here and cms/forms.py
        """
        if data_structure.type in[DataStructure.DATA_TYPES.image, DataStructure.DATA_TYPES.file]:
            # If a file has been uploaded try to save it
            if data_structure_name in request_files:
                if request_files[data_structure_name]:
                    new_record_value, file_errors = upload_file(data_structure, request_files[data_structure_name])
                    if file_errors:
                        upload_errors.extend(file_errors)
                        continue

            # No file was uploaded and there is a record means the user didn't change anything so skip
            elif not data_structure.optional and latest_value:
                continue
            # No file was uploaded and the user didn't delete an optional data structure so skip
            elif data_structure.optional and 'delete_' + data_structure_name not in request_data:
                continue

        elif data_structure.type == DataStructure.DATA_TYPES.guid:

            # if the guid is valid it will go to the next set of checks
            new_record_value = request_data[data_structure_name] if data_structure_name in request_data else ""
            is_guid = re.match('\{[\da-fA-F]{8}-[\da-fA-F]{4}-[\da-fA-F]{4}-[\da-fA-F]{4}-[\da-fA-F]{12}\}',
                               new_record_value)

            # if its option and not a valid guid set error message and go to next DataStructure
            if new_record_value and not is_guid:
                upload_errors.append((data_structure_name, 'Invalid GUID {} it should formatted like {}'
                                      .format(new_record_value, "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXX}")))
                continue

            # no guid submitted or default value and is not optional generate a guid
            elif not data_structure.optional and not new_record_value:  # and not data_structure.default:
                new_record_value = '{' + str(uuid.uuid4()) + '}'
                upload_errors.append((data_structure_name,
                                      'No submitted GUID or default value. GUID has been generated as {}'
                                      .format(new_record_value)))

        elif data_structure.type == DataStructure.DATA_TYPES.select:
            values = request_data.getlist(data_structure_name)
            if 'multi' in data_structure.meta_settings and data_structure.meta_settings['multi']:
                new_record_value = json.dumps(values)
            else:
                new_record_value = values[0]

        elif data_structure.type in [DataStructure.DATA_TYPES.external_file, DataStructure.DATA_TYPES.external_image]:

            # If the user uploads a new file create a new ExternalFile record
            if data_structure_name in request_files:
                request_file = request_files[data_structure_name]

                file_errors = check_meta_settings(data_structure, request_file)
                if file_errors:
                    upload_errors.extend(file_errors)
                    continue

                md5 = hashlib.md5()
                for chunk in request_file.chunks():
                    md5.update(chunk)

                external_file = ExternalFile(data_structure=data_structure, product=product)
                external_file.save()

                external_file.file = request_file
                external_file.md5 = md5.hexdigest()
                external_file.size = request_file.size
                external_file.save()

                new_record_value = external_file.file.url
            # No file was uploaded and there is a record means the user didn't change anything so skip
            elif not data_structure.optional and latest_value:
                continue
            # No file was uploaded and the user didn't delete an optional data structure so skip
            elif data_structure.optional and 'delete_' + data_structure_name not in request_data:
                continue

            # Mark this file for deletion
            elif 'delete_' + data_structure_name in request_data and request_data['delete_' + data_structure_name]:
                delete_file = True

        elif data_structure.type == DataStructure.DATA_TYPES.check_box:
            new_record_value = data_structure_name in request_data

        elif data_structure.type in [DataStructure.DATA_TYPES.object, DataStructure.DATA_TYPES.array]:
            new_record_value = request_data[data_structure_name]
            try:
                yaml.safe_load(new_record_value)
            except yaml.YAMLError:
                upload_errors.append((data_structure_name, "Json was incorrectly formatted."))
                continue

        elif data_structure_name in request_data:
            new_record_value = request_data[data_structure_name]
            if data_structure.type == DataStructure.DATA_TYPES.text and 'regex' in data_structure.meta_settings:
                pattern = data_structure.meta_settings['regex']
                if new_record_value and not re.match(pattern, new_record_value):
                    upload_errors.append((data_structure_name, 'Invalid input'))
                    continue
            elif 'char_limit' in data_structure.meta_settings:
                char_limit = int(data_structure.meta_settings['char_limit'])
                if len(new_record_value) > char_limit:
                    upload_errors.append(
                        (data_structure_name,
                         'Character limit exceeded. Text was {} characters but should not be more than {} characters'.
                         format(len(new_record_value), char_limit)))

        # If the data structure is not option and no record exists and nothing was uploaded try to use the default value
        if not data_structure.optional and not new_record_value:
            if not latest_value:
                new_record_value = data_structure.default
                if new_record_value:
                    upload_errors.append((data_structure_name, "This field cannot be blank. Using default value"))
                else:
                    upload_errors.append((data_structure_name, "This field cannot be blank"))
                    continue
            else:
                continue

        if new_record_value == latest_value and not delete_file and not external_file:
            continue

        if data_structure.advanced and not (user.is_superuser or user.has_perm('cms.edit_advanced')):
            upload_errors.append((data_structure_name, "You do not have permission to edit this field"))
            continue

        record = DataRecord(product=product,
                            data_structure=data_structure,
                            language=ds_language,
                            value=new_record_value,
                            created_by=user)
        record.save()

        if external_file:
            record.external_file = external_file
            record.save()

    fill_content(product,
                 preview=True,
                 incremental=True,
                 version_id=version_id,
                 changed_context=context)

    return upload_errors


def update_latest_record_version(records, new_version):
    record = records.latest('created_date')
    if not record.version:
        record.version = new_version
        record.save()


def update_records_to_version(product, contexts, version):
    languages = Language.objects.all()
    for context in contexts:
        for data_structure in context.datastructure_set.all():
            all_records = data_structure.datarecord_set.filter(product=product)

            if data_structure.translatable:
                for language in languages:
                    records_for_language = all_records.filter(language=language)
                    # Now only the latest records that can be published will have its
                    # version altered
                    if records_for_language.exists():
                        update_latest_record_version(records_for_language, version)

            elif all_records.exists():
                update_latest_record_version(all_records, version)


def strip_version_from_records(version, product):
    records_to_strip = DataRecord.objects.filter(version=version, product=product)
    for record in records_to_strip:
        record.version = None
        record.save()


# Currently unused
def remove_unused_records(product):
    nullify_records = DataRecord.objects.filter(product=product, version_id=None)
    if nullify_records.exists():
        for record in nullify_records:
            record.delete()


def generate_preview_link(context=None):
    return context.url + "?preview" if context else "/content/about?preview"


def generate_preview(product, context=None, version_id=None, send_to_review=False):
    fill_content(product,
                 preview=True,
                 incremental=True,
                 changed_context=context,
                 version_id=version_id,
                 send_to_review=send_to_review)
    return generate_preview_link(context)


def publish_latest_version(product, review_id, user):
    publish_errors = update_draft_state(review_id, ProductCustomizationReview.REVIEW_STATES.accepted, user)
    if not publish_errors:
        fill_content(product, preview=False, incremental=True)
    return publish_errors


def integration_has_required_data(product):
    errors = []
    for datastructure in DataStructure.objects.filter(context__product_type__product=product):
        if not datastructure.optional and not datastructure.datarecord_set.exists():
            ds_name = datastructure.label if datastructure.label else datastructure.name
            errors.append((ds_name,
                           "This field cannot be blank. Go to the {} page and fill in {}.".
                           format(datastructure.context.name, ds_name)))
    return errors


def send_version_for_review(product, user):
    old_versions = ContentVersion.objects.filter(product=product, accepted_date=None)

    if old_versions.exists():
        old_version = old_versions.latest('created_date')
        strip_version_from_records(old_version, product)
        old_version.delete()

    # We only check for integrations because its the only product type that non staff have access to.
    if product.product_type.type == ProductType.PRODUCT_TYPES.integration:
        errors = integration_has_required_data(product)
        if len(errors) > 0:
            return errors

    version = ContentVersion(product=product, created_by=user)
    version.save()

    version.create_reviews()

    update_records_to_version(product, Context.objects.filter(product_type=product.product_type), version)

    notify_version_ready(product, version.id, user)
    return []


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


# File upload helpers
def is_not_valid_file_type(file_type, meta_types):
    for meta_type in meta_types.split(','):
        if meta_type.strip() in file_type:
            return False
    return True


def get_image_dimensions(image_file):
    new_image = Image.open(image_file)
    width, height = new_image.size
    return {'width': width, 'height': height}


def check_image_dimensions(data_structure_name,
                           meta_dimensions, image_dimensions):
    size_error_msgs = []
    if not meta_dimensions:
        return size_error_msgs

    if 'height' in meta_dimensions and meta_dimensions['height'] != image_dimensions['height']:
        error_msg = "Image height must be equal to {}. Uploaded image's height is {}."\
            .format(meta_dimensions['height'], image_dimensions['height'])
        size_error_msgs.append((data_structure_name, error_msg))

    if 'width' in meta_dimensions and meta_dimensions['width'] != image_dimensions['width']:
        error_msg = "Image width must be equal to {}. Uploaded image's width is {}."\
            .format(meta_dimensions['width'], image_dimensions['width'])
        size_error_msgs.append((data_structure_name, error_msg))

    if 'height_le' in meta_dimensions and meta_dimensions['height_le'] < image_dimensions['height']:
        error_msg = "Image height must be equal to or less than {}. Uploaded image's height is {}."\
            .format(meta_dimensions['height_le'], image_dimensions['height'])
        size_error_msgs.append((data_structure_name, error_msg))

    if 'width_le' in meta_dimensions and meta_dimensions['width_le'] < image_dimensions['width']:
        error_msg = "Image width must be equal to or less than {}. Uploaded image's width is {}."\
            .format(meta_dimensions['width_le'], image_dimensions['width'])
        size_error_msgs.append((data_structure_name, error_msg))

    if 'height_ge' in meta_dimensions and meta_dimensions['height_ge'] > image_dimensions['height']:
        error_msg = "Image height must be equal to or more than {}. Uploaded image's height is {}."\
            .format(meta_dimensions['height_ge'], image_dimensions['height'])
        size_error_msgs.append((data_structure_name, error_msg))

    if 'width_ge' in meta_dimensions and meta_dimensions['width_ge'] > image_dimensions['width']:
        error_msg = "Image width must be equal to or more than {}. Uploaded image's width is {}." \
            .format(meta_dimensions['width_ge'], image_dimensions['width'])
        size_error_msgs.append((data_structure_name, error_msg))

    return size_error_msgs


def check_meta_settings(data_structure, new_file):
    meta_settings = data_structure.meta_settings
    if 'format' in meta_settings and is_not_valid_file_type(new_file.content_type, meta_settings['format']):
        error_msg = "Invalid file type. Uploaded file is {}. It should be {}." \
            .format(new_file.content_type,
                    data_structure.meta_settings['format'].replace(',', ' or '))
        return [(data_structure.name, error_msg)]

    if 'size' in meta_settings and meta_settings['size'] < new_file.size:
        error_msg = "The file's size it too large. Its size was {0:.2f}MB but must be less than {1:.2f}MB".\
            format(new_file.size/BYTES_TO_MEGABYTES, meta_settings['size']/BYTES_TO_MEGABYTES)
        return [(data_structure.name, error_msg)]

    if data_structure.type in [DataStructure.DATA_TYPES.image, DataStructure.DATA_TYPES.external_image]:

        try:
            image_dimensions = get_image_dimensions(new_file)
        except (IOError, TypeError):
            return [(data_structure.name, "Image is damaged please upload an valid version")]

        return check_image_dimensions(data_structure.name, meta_settings, image_dimensions)

    return []


# End of file upload helpers
def upload_file(data_structure, new_file):
    encoded_file = base64.b64encode(new_file.read())
    if new_file.size >= settings.CMS_MAX_FILE_SIZE:
        return None, [(data_structure.name, 'Its size was {0:.2f}MB but must be less than {1:.2f} MB'.
                       format(new_file.size/BYTES_TO_MEGABYTES, settings.CMS_MAX_FILE_SIZE/BYTES_TO_MEGABYTES))]

    file_errors = check_meta_settings(data_structure, new_file)
    if file_errors:
        return None, file_errors
    return encoded_file, []
