from django import template
from ..models import *

register = template.Library()


@register.filter
def is_ImageField(field):
    return type(field.field).__name__ == "ImageField"


@register.filter
def is_FileField(field):
    return type(field.field).__name__ == "FileField"


@register.simple_tag
def get_data_structure(data_structure_name, context):
    query = DataStructure.objects.filter(context=context, name=data_structure_name)
    if query.exists():
        return query[0]
    return None


@register.simple_tag
def is_external_file_or_image(data_structure_name, context):
    query = DataStructure.objects.filter(context=context, name=data_structure_name)
    if query.exists():
        return query[0].type in [DataStructure.DATA_TYPES.external_file, DataStructure.DATA_TYPES.external_image]
    return False


@register.simple_tag
def has_value(data_structure_name, product, context, language):
    query = DataStructure.objects.filter(context=context, name=data_structure_name)

    if query.exists():
        data_structure = query[0]
    else:
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
def has_permission(user, permission, customization=settings.CUSTOMIZATION):
    return UserGroupsToProductPermissions.check_customization_permission(user, customization, permission)


@register.filter
def modulo(value, arg):
    return int(value) % int(arg)


@register.filter
def anon_notes(notes):
    return ProductCustomizationReview.anon_notes(notes)

