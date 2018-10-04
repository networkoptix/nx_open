from datetime import datetime

from notifications.api import send
from django.contrib.auth.models import Permission
from django.db.models import Q

from PIL import Image
import base64, json, re, uuid

from .filldata import fill_content
from api.models import Account
from ..models import *


def update_draft_state(review_id, target_state, user):
    review = ProductCustomizationReview.objects.filter(id=review_id, reviewed_by=None)
    if not review.exists():
        return " is currently publishing or has already been published"

    review = review.latest('id')

    if not review.version.accepted_by:
        review.version.accepted_by = user
        review.version.accepted_date = datetime.now()
        review.version.save()

    review.reviewed_by = user
    review.reviewed_date = datetime.now()
    review.state = target_state
    review.save()
    return None


def notify_version_ready(product, version_id, exclude_user):
    perm = Permission.objects.get(codename='publish_version')
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
        latest_value = data_structure.find_actual_value(product, ds_language)
        # If the DataStructure is supposed to be an image convert to base64 and
        # error check
        # TODO: Refactor image/file logic - CLOUD-1524
        """
            Currently if the data structure is optional you can remove the value.
            
            Planned change is to make it to where you can "delete" the value and if its not optional then fallback
            to the default value.
            
            This will create a new record making images/files behave like the other data structure types
            Places to touch are here, cms/forms.py, and cms/templates/context_editor.html
        """
        if data_structure.type == DataStructure.DATA_TYPES.image\
                or data_structure.type == DataStructure.DATA_TYPES.file:
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

        elif data_structure_name in request_data:
            new_record_value = request_data[data_structure_name]

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

        if new_record_value == latest_value:
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


def send_version_for_review(product, user):
    old_versions = ContentVersion.objects.filter(product=product, accepted_date=None)

    if old_versions.exists():
        old_version = old_versions.latest('created_date')
        strip_version_from_records(old_version, product)
        old_version.delete()

    version = ContentVersion(product=product, created_by=user)
    version.save()

    version.create_reviews()

    update_records_to_version(product, Context.objects.filter(product_type=product.product_type), version)

    notify_version_ready(product, version.id, user)


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


def is_not_valid_file_type(file_type, meta_types):
    for meta_type in meta_types.split(','):
        if meta_type.strip() in file_type:
            return False
    return True


def upload_file(data_structure, file):
    file_errors = []
    file_size = file.size / 1048576.0
    if file_size >= settings.CMS_MAX_FILE_SIZE:
        file_errors.append((data_structure.name, 'Its size was {}MB but must be less than {} MB'
                            .format(file_size, settings.CMS_MAX_FILE_SIZE)))
        return None, file_errors

    try:
        formats = []
        if 'format' in data_structure.meta_settings:
            formats = data_structure.meta_settings['format']
        encoded_file, file_dimensions = encode_file(file, data_structure.type, formats)
    except (IOError, TypeError):
        file_errors.append((data_structure.name, "Image is damaged please upload an valid version"))
        return None, file_errors

    if not encoded_file:
        error_msg = "Invalid file type. Uploaded file is {}. It should be {}." \
            .format(file.content_type,
                    data_structure.meta_settings['format'].replace(',', ' or '))
        file_errors.append((data_structure.name, error_msg))
        return None, file_errors

    # Gets the meta_settings form the DataStructure to check if the sizes are valid
    # if the length is zero then there is no meta settings
    if encoded_file and len(data_structure.meta_settings):
        if data_structure.type == DataStructure.DATA_TYPES.image:
            file_errors = check_image_dimensions(data_structure.name, data_structure.meta_settings, file_dimensions)

        if 'size' in data_structure.meta_settings \
                and file_size > float(data_structure.meta_settings['size']):
            file_errors.append((data_structure.name, 'File size is {} MB it should be less than {} MB'
                                .format(file_size, data_structure.meta_settings['size'])))
    if file_errors:
        return None, file_errors

    return encoded_file, []


def encode_file(file, data_structure_type, valid_formats):
    encoded_string = base64.b64encode(file.read())
    file_type = file.content_type
    file_dimensions = None

    if valid_formats and is_not_valid_file_type(file_type.lower(), valid_formats):
        return None, None

    if data_structure_type == DataStructure.DATA_TYPES.image:
        new_image = Image.open(file)
        width, height = new_image.size
        file_dimensions = {'width': width, 'height': height}

    return encoded_string, file_dimensions


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
