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
    return DataStructure.objects.get(context=context, name=data_structure_name).optional


@register.simple_tag
def has_value(data_structure_name, context, customization, language):
    data_structure = DataStructure.objects.get(context=context, name=data_structure_name)

    if not data_structure.translatable:
        language = None

    data_records = DataRecord.objects.filter(data_structure=data_structure,
                                             customization=customization,
                                             language=language)
    return data_records.exists()
