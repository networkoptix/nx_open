from __future__ import unicode_literals
from django.contrib import admin
from django.urls import reverse
from django.utils.html import format_html
from .forms import *
from cloud import settings


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
        if not UserGroupsToCustomizationPermissions.check_permission(request.user, settings.CUSTOMIZATION):
            # return empty dataset, only superuser can watch content in other
            # customizations
            return qs.filter(pk=-1)
        return qs


class ProductAdmin(CMSAdmin):
    list_display = ('context_actions', 'name', 'product_type', 'customizations_list')
    list_display_links = ('name',)
    list_filter = ('product_type', )
    form = ProductForm

    def context_actions(self, obj):
        return format_html('<a class="btn btn-sm" href="{}">settings</a>',
                           reverse('product_settings', args=[obj.id]))

    context_actions.short_description = 'Admin Options'
    context_actions.allow_tags = True

    def customizations_list(self, obj):
        return ", ".join(obj.customizations.values_list('name', flat=True))
    context_actions.short_description = 'Customizations'


admin.site.register(Product, ProductAdmin)


class ContextAdmin(CMSAdmin):
    list_display = ('context_actions', 'name', 'description',
                    'url', 'translatable', 'is_global', 'product_type')

    list_display_links = ('name',)
    list_filter = ('product_type',)
    search_fields = ('name', 'description', 'url')

    def changelist_view(self, request, extra_context=None):
        if not request.user.is_superuser:
            self.list_display_links = (None,)
        return super(ContextAdmin, self).changelist_view(request, extra_context)

    def context_actions(self, obj):
        return format_html('<a class="btn btn-sm" href="{}">edit content</a>',
                           reverse('page_editor', args=[obj.id]))

    def get_queryset(self, request):  # show only users for cloud_portal product type
        qs = super(ContextAdmin, self).get_queryset(request)  # Basic check from CMSAdmin
        if not request.user.is_superuser:
            qs = qs.filter(hidden=False)  # only superuser sees hidden contexts
        return qs

    context_actions.short_description = 'Admin Options'
    context_actions.allow_tags = True


admin.site.register(Context, ContextAdmin)


class ContextTemplateAdmin(CMSAdmin):
    list_display = ('context', 'language')
    list_filter = ('context', 'language')
    search_fields = ('context__name', 'context__file_path', 'language__code')


admin.site.register(ContextTemplate, ContextTemplateAdmin)


class DataStructureAdmin(CMSAdmin):
    list_display = ('context', 'label', 'name', 'description', 'translatable', 'type')
    list_filter = ('context', 'translatable', 'context__product_type')
    search_fields = ('context__name', 'name', 'description', 'type')


admin.site.register(DataStructure, DataStructureAdmin)


class LanguageAdmin(CMSAdmin):
    list_display = ('name', 'code')


admin.site.register(Language, LanguageAdmin)


class CustomizationAdmin(CMSAdmin):
    list_display = ('name',)


admin.site.register(Customization, CustomizationAdmin)


class DataRecordAdmin(CMSAdmin):
    list_display = ('product', 'language', 'context',
                    'data_structure', 'short_description', 'version')
    list_filter = ('product', 'language', 'data_structure__context', 'data_structure')
    search_fields = ('data_structure__context__name', 'data_structure__name',
                     'data_structure__description', 'value', 'language__code')
    readonly_fields = ('created_by',)


admin.site.register(DataRecord, DataRecordAdmin)


class ContentVersionAdmin(CMSAdmin):
    list_display = ('content_version_actions', 'id', 'product',
                    'created_date', 'created_by',
                    'accepted_date', 'accepted_by', 'state')

    list_display_links = ('id', )
    list_filter = ('product',)
    search_fields = ('accepted_by__email', 'created_by__email')
    readonly_fields = ('created_by', 'accepted_by',)

    def changelist_view(self, request, extra_context=None):
        if not request.user.is_superuser:
            self.list_display_links = (None,)
        return super(ContentVersionAdmin, self).changelist_view(request, extra_context)

    def content_version_actions(self, obj):
        return format_html('<a class="btn btn-sm" href="{}">review</a>',
                           reverse('version', args=[obj.id]))

    def get_queryset(self, request):  # show only users for current cloud_portal product
        qs = super(ContentVersionAdmin, self).get_queryset(request)  # Basic check from CMSAdmin
        qs = qs.filter(product=get_cloud_portal_product())
        return qs

    content_version_actions.short_description = "Admin Options"
    content_version_actions.allow_tags = True


admin.site.register(ContentVersion, ContentVersionAdmin)


class UserGroupsToCustomizationPermissionsAdmin(CMSAdmin):
    list_display = ('id', 'group', 'customization',)


admin.site.register(UserGroupsToCustomizationPermissions, UserGroupsToCustomizationPermissionsAdmin)
