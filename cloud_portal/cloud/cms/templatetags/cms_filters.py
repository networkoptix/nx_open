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
def is_optional(data_structure_name, context):
    query = DataStructure.objects.filter(context=context, name=data_structure_name)
    if query.exists():
        return query[0].optional
    return False


@register.simple_tag
def has_value(data_structure_name, context, customization, language):
    query = DataStructure.objects.filter(context=context, name=data_structure_name)

    if query.exists():
        data_structure = query[0]
    else:
        return False

    if not data_structure.translatable:
        language = None

    data_records = DataRecord.objects.filter(data_structure=data_structure,
                                             customization=customization,
                                             language=language)
    return data_records.exists()
