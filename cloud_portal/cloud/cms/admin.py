from __future__ import unicode_literals
from django.contrib import admin
from django.utils.html import format_html
from models import *


# CMS structure (data structure). Only developers can change that

class ProductAdmin(admin.ModelAdmin):
    list_display = ('name',)
admin.site.register(Product, ProductAdmin)


class ContextAdmin(admin.ModelAdmin):
    list_display = ('product', 'name', 'description', 'url', 'translatable', 'context_actions')

    def context_actions(self, obj):
        return format_html('<a class="button" href="/admin/cms/context_editor/{}/">edit content</a>',
                            obj.id)

    context_actions.short_description = 'Admin Options'
    context_actions.allow_tags = True

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
                    'accepted_date', 'accepted_by',
                    'content_version_actions')

    def content_version_actions(self, obj):
        return format_html('<a class="button" href="/admin/cms/review_version/{}/">review version</a>',
                            obj.id)

    content_version_actions.short_description = "Admin Options"
    content_version_actions.allow_tags = True

admin.site.register(ContentVersion, ContentVersionAdmin)

