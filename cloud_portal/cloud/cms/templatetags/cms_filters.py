from django import template
from ..models import *

register = template.Library()


@register.filter
def is_ImageField(field):
    return type(field.field).__name__ == "ImageField"


@register.filter
def is_FileField(field):
    return type(field.field).__name__ == "FileField"


@register.filter
def is_optional(field, context):
    name = field.name
    data_structure = DataStructure.objects.get(context=context, name=name)
    return data_structure.optional


@register.filter
def has_value(field, context):
    name = field.name
    data_structure = DataStructure.objects.get(context=context, name=name)

    data_records = DataRecord.objects.filter(data_structure=data_structure)

    return data_records.exists()
