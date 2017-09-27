from django import template
from ..models import *

register = template.Library()


@register.filter
def is_ImageField(data_structure_name):
    return type(data_structure_name.field).__name__ == "ImageField"


@register.filter
def is_FileField(data_structue_name):
    return type(data_structue_name.field).__name__ == "FileField"

@register.filter
def get_FileName(data_structure_name):
    label = data_structure_name.label
    
    data_structure = DataStructure.objects.filter(label=label)
    
    if data_structure.exists():
        return data_structure.get().name
    
    return label
