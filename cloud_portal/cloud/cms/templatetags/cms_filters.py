from django import template
from ..models import *

import json

register = template.Library()


@register.filter
def is_ImageField(field):
    return type(field.field).__name__ == "ImageField"


@register.filter
def is_FileField(field):
    return type(field.field).__name__ == "FileField"


@register.simple_tag
def get_data_structure(data_structure_name, context):
    return DataStructure.objects.filter(context=context, name=data_structure_name).first()


@register.simple_tag
def is_external_file_or_image(data_structure_name, context):
    query = DataStructure.objects.filter(context=context, name=data_structure_name).first()
    if query:
        return query.type in [DataStructure.DATA_TYPES.external_file, DataStructure.DATA_TYPES.external_image]
    return False


@register.simple_tag
def has_value(data_structure_name, product, context, language):
    data_structure = DataStructure.objects.filter(context=context, name=data_structure_name).first()

    if not data_structure:
        return False

    if not data_structure.translatable:
        language = None

    record_value = data_structure.find_actual_value(product, language, draft=True)
    return record_value != "" and data_structure.default != record_value


@register.simple_tag
def get_datastructure_type(data_structure):
    return DataStructure.DATA_TYPES[data_structure.type] if data_structure else 0


@register.simple_tag
def get_product_type(product):
    if product:
        return ProductType.PRODUCT_TYPES[product.product_type.type]
    return product


@register.simple_tag
def get_review_state(state):
    return ProductCustomizationReview.REVIEW_STATES[state]


@register.simple_tag
def has_permission(user, product, permission=None):
    return UserGroupsToProductPermissions.check_permission(user, product, permission)


@register.simple_tag
def has_customization_permission(user, customization, permission):
    if not customization:
        customization = settings.CUSTOMIZATION
    return UserGroupsToProductPermissions.check_customization_permission(user, customization, permission)


@register.filter
def modulo(value, arg):
    return int(value) % int(arg)


@register.filter
def nice_multiselect(multiselect_record):
    return ', '.join(json.loads(multiselect_record.value))
