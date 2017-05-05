from __future__ import unicode_literals
from django.contrib import admin
from models import *


# CMS structure (data structure). Only developers can change that

class ProductAdmin(admin.ModelAdmin):
    pass
admin.site.register(Product, ProductAdmin)


class ContextAdmin(admin.ModelAdmin):
    pass
admin.site.register(Context, ContextAdmin)


class DataStructureAdmin(admin.ModelAdmin):
    pass
admin.site.register(DataStructure, DataStructureAdmin)


class LanguageAdmin(admin.ModelAdmin):
    pass
admin.site.register(Language, LanguageAdmin)


class CustomizationAdmin(admin.ModelAdmin):
    pass
admin.site.register(Customization, CustomizationAdmin)


class DataRecordAdmin(admin.ModelAdmin):
    pass
admin.site.register(DataRecord, DataRecordAdmin)


class ContentVersionAdmin(admin.ModelAdmin):
    pass
admin.site.register(ContentVersion, ContentVersionAdmin)

