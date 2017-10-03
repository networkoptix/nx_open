from __future__ import unicode_literals
from django.contrib import admin
from django.urls import reverse
from django.utils.html import format_html
from models import *
from cloud import settings
from django.contrib import admin

admin.site.site_header = 'Cloud Administration'
admin.site.site_title = 'Cloud Administration'
admin.site.index_title = 'Cloud Administration'


class CMSAdmin(admin.ModelAdmin):
    # this class protects us from user error:
    # 1. only superuser can edit specific data in CMS (get_readonly_fields, has_add_permission, has_delete_permission)
    # 2. customization admins cannot see anything in another customizations (get_queryset)
    def get_readonly_fields(self, request, obj=None):
        if request.user.is_superuser:
            return list(self.readonly_fields)
        return list(set(list(self.readonly_fields) +
                        [field.name for field in obj._meta.fields] +
                        [field.name for field in obj._meta.many_to_many]))

    def has_add_permission(self, request):
        return request.user.is_superuser

    def has_delete_permission(self, request, obj=None):
        return request.user.is_superuser

    def get_queryset(self, request):
        qs = super(CMSAdmin, self).get_queryset(request)
        if not request.user.is_superuser and \
           request.user.customization != settings.CUSTOMIZATION:
            # return empty dataset, only superuser can watch content in other
            # customizations
            return qs.filter(pk=-1)
        return qs


class ProductAdmin(CMSAdmin):
    list_display = ('context_actions', 'name',)
    list_display_links = ('name',)

    def context_actions(self, obj):
        return format_html('<button class="btn"><a href="{}">settings</a></button>',
                           reverse('product_settings', args=[obj.id]))

    context_actions.short_description = 'Admin Options'
    context_actions.allow_tags = True


admin.site.register(Product, ProductAdmin)


class ContextAdmin(CMSAdmin):
    list_display = ('context_actions', 'product', 'name', 'description',
                    'url', 'translatable', 'is_global')

    list_display_links = ('name', )
    search_fields = ('name', 'description', 'url', 'product__name')

    def context_actions(self, obj):
        return format_html('<button class="btn"><a href="{}">edit content</a></button>',
                           reverse('context_editor', args=[obj.id]))

    def get_queryset(self, request):  # show only users for current customization
        qs = super(ContextAdmin, self).get_queryset(request)  # Basic check from CMSAdmin
        if not request.user.is_superuser:
            qs = qs.filter(hidden=False)  ## only superuser sees hidden contexts
        # additional filter - display only revisions from current customization
        return qs

    context_actions.short_description = 'Admin Options'
    context_actions.allow_tags = True

admin.site.register(Context, ContextAdmin)


class DataStructureAdmin(CMSAdmin):
    list_display = ('context', 'name', 'description', 'translatable', 'type')
    list_filter = ('context', 'translatable')
    search_fields = ('context__name', 'name', 'description', 'type')

admin.site.register(DataStructure, DataStructureAdmin)


class LanguageAdmin(CMSAdmin):
    list_display = ('name', 'code')
admin.site.register(Language, LanguageAdmin)


class CustomizationAdmin(CMSAdmin):
    list_display = ('name',)
admin.site.register(Customization, CustomizationAdmin)


class DataRecordAdmin(CMSAdmin):
    list_display = ('customization', 'language',
                    'data_structure', 'short_description', 'version')
    list_filter = ('data_structure', 'customization', 'language')
    search_fields = ('data_structure__name', 'customization__name',
                     'data_structure__description', 'value', 'language__code')

admin.site.register(DataRecord, DataRecordAdmin)


class ContentVersionAdmin(CMSAdmin):
    list_display = ('content_version_actions', 'id', 'customization', 'name',
                    'created_date', 'created_by',
                    'accepted_date', 'accepted_by', 'state')

    list_display_links = ('id', )

    def content_version_actions(self, obj):
        return format_html('<button class="btn"> <a href="{}">review version</a></button>',
                           reverse('review_version', args=[obj.id]))

    def get_queryset(self, request):  # show only users for current customization
        qs = super(ContentVersionAdmin, self).get_queryset(request)  # Basic check from CMSAdmin
        qs = qs.filter(customization__name=settings.CUSTOMIZATION)
        # additional filter - display only revisions from current customization
        return qs

    content_version_actions.short_description = "Admin Options"
    content_version_actions.allow_tags = True

admin.site.register(ContentVersion, ContentVersionAdmin)
