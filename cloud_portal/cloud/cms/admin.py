from __future__ import unicode_literals
from django.contrib import admin
from models import *


# CMS structure (data structure). Only developers can change that

class ProductAdmin(admin.ModelAdmin):
    list_display = [f.name for f in Product._meta.get_fields()]
admin.site.register(Product, ProductAdmin)


class ContextAdmin(admin.ModelAdmin):
    list_display = [f.name for f in Context._meta.get_fields()]
admin.site.register(Context, ContextAdmin)


class DataStructureAdmin(admin.ModelAdmin):
    list_display = [f.name for f in DataStructure._meta.get_fields()]
admin.site.register(DataStructure, DataStructureAdmin)


class LanguageAdmin(admin.ModelAdmin):
    list_display = [f.name for f in Language._meta.get_fields()]
admin.site.register(Language, LanguageAdmin)


class CustomizationAdmin(admin.ModelAdmin):
    list_display = [f.name for f in Customization._meta.get_fields()]
admin.site.register(Customization, CustomizationAdmin)


class DataRecordAdmin(admin.ModelAdmin):
    list_display = [f.name for f in DataRecord._meta.get_fields()]
admin.site.register(DataRecord, DataRecordAdmin)


class ContentVersionAdmin(admin.ModelAdmin):
    list_display = [f.name for f in ContentVersion._meta.get_fields()]
admin.site.register(ContentVersion, ContentVersionAdmin)

