from __future__ import unicode_literals
from django.contrib import admin
from models import *


# CMS structure (data structure). Only developers can change that

class ProductAdmin(admin.ModelAdmin):
    list_display = ('name',)
admin.site.register(Product, ProductAdmin)


class ContextAdmin(admin.ModelAdmin):
    list_display = ('product', 'name', 'description', 'url', 'translatable')
admin.site.register(Context, ContextAdmin)


class DataStructureAdmin(admin.ModelAdmin):
    list_display = ('context', 'name', 'description', 'translatable')

admin.site.register(DataStructure, DataStructureAdmin)


class LanguageAdmin(admin.ModelAdmin):
    list_display = ('name', 'code')
admin.site.register(Language, LanguageAdmin)


class CustomizationAdmin(admin.ModelAdmin):
    list_display = ('name',)
admin.site.register(Customization, CustomizationAdmin)


class DataRecordAdmin(admin.ModelAdmin):
    list_display = ('customization', 'language', 'data_structure', 'value', 'version')
admin.site.register(DataRecord, DataRecordAdmin)


class ContentVersionAdmin(admin.ModelAdmin):
    list_display = ('id', 'customization', 'name',
                    'created_date', 'created_by',
                    'accepted_date', 'accepted_by')
admin.site.register(ContentVersion, ContentVersionAdmin)

