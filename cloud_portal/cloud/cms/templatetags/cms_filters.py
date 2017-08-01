from django import template

register = template.Library()

@register.filter
def is_ImageField(data_structure_name):
	return type(data_structure_name.field).__name__ == "ImageField"
