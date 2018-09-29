from __future__ import unicode_literals
from django.contrib import admin
from django.contrib.admin import SimpleListFilter
from django.urls import reverse
from django.utils.html import format_html

from datetime import datetime

from cloud import settings
from cms.forms import *
from cms.controllers.modify_db import get_records_for_version


class CloudFilter(SimpleListFilter):
    title = 'Product'
    parameter_name = 'product'

    def lookups(self, request, model_admin):
        products = Product.objects.filter(product_type__type=ProductType.PRODUCT_TYPES.cloud_portal)
        if not request.user.is_superuser:
            products = products.filter(customizations__name__in=[settings.CUSTOMIZATION])
            return [(p.id, p.name) for p in products]
        return [(p.id, p.__str__()) for p in products]

    def queryset(self, request, queryset):
        if self.value():
            return queryset.filter(product=self.value())
        return queryset


class ProductFilter(SimpleListFilter):
    title = 'Product'
    parameter_name = 'product'

    def lookups(self, request, model_admin):
        products = Product.objects.exclude(product_type__type=ProductType.PRODUCT_TYPES.cloud_portal)
        if not request.user.is_superuser:
            products = products.filter(customizations__name__in=[settings.CUSTOMIZATION])
            return [(p.id, p.name) for p in products]
        return [(p.id, p.__str__()) for p in products]

    def queryset(self, request, queryset):
        if self.value():
            return queryset.filter(product=self.value())
        return queryset


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


class ProductTypeAdmin(CMSAdmin):
    list_display = ('type', 'single_customization',)


admin.site.register(ProductType, ProductTypeAdmin)


class ProductAdmin(CMSAdmin):
    list_display = ('product_actions', 'name', 'product_type', 'customizations_list')
    list_display_links = ('name',)
    list_filter = ('product_type', )
    form = ProductForm
    change_form_template = 'cms/product_change_form.html'

    def change_view(self, request, object_id, form_url='', extra_context=None):
        extra_context = extra_context or {}
        extra_context['current_versions'] = []
        approved = ProductCustomizationReview.REVIEW_STATES.accepted
        product = Product.objects.get(id=object_id)
        product_customizations = ProductCustomizationReview.objects.filter(version__product=product)
        for customization in product.customizations.all():
            approved_versions = product_customizations.filter(customization=customization, state=approved)
            if approved_versions.exists():
                extra_context['current_versions'].append(approved_versions.latest('id'))
            else:
                extra_context['current_versions'].append({'customization': customization.name,
                                                          'id': None})

        return super(ProductAdmin, self).change_view(
            request, object_id, form_url, extra_context=extra_context,
        )

    def product_actions(self, obj):
        if not obj.product_type or obj.product_type.type == ProductType.PRODUCT_TYPES.cloud_portal:
            return format_html('<a class="btn btn-sm" href="{}">settings</a>',
                               reverse('product_settings', args=[obj.id]))
        context = Context.objects.get(product_type=obj.product_type)
        return format_html('<a class="btn btn-sm" href="{}">edit information</a>',
                           reverse('page_editor', args=[context.id, None, obj.id]))

    product_actions.short_description = ''
    product_actions.allow_tags = True

    def customizations_list(self, obj):
        return ", ".join(obj.customizations.values_list('name', flat=True))


admin.site.register(Product, ProductAdmin)


class ContextAdmin(CMSAdmin):
    list_display = ('context_actions', 'name', 'description',
                    'url', 'translatable', 'is_global')

    list_display_links = ('name',)
    search_fields = ('name', 'description', 'url')

    def changelist_view(self, request, extra_context=None):
        if not request.user.is_superuser:
            self.list_display_links = (None,)
        else:
            self.list_filter = ('product_type', )
        return super(ContextAdmin, self).changelist_view(request, extra_context)

    def context_actions(self, obj):
        return format_html('<a class="btn btn-sm" href="{}">edit content</a>',
                           reverse('page_editor', args=[obj.id]))

    def get_queryset(self, request):  # show only users for cloud_portal product type
        qs = super(ContextAdmin, self).get_queryset(request)  # Basic check from CMSAdmin
        if not request.user.is_superuser:
            qs = qs.filter(product_type__type=ProductType.PRODUCT_TYPES.cloud_portal).\
                filter(hidden=False)  # only superuser sees hidden contexts
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
    list_filter = (CloudFilter,)
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
        qs = qs.filter(product__product_type__type=ProductType.PRODUCT_TYPES.cloud_portal)
        if not request.user.is_superuser:
            qs = qs.filter(product__customizations__name__in=[settings.CUSTOMIZATION])
        return qs

    content_version_actions.short_description = "Admin Options"
    content_version_actions.allow_tags = True


admin.site.register(ContentVersion, ContentVersionAdmin)


class ProductCustomizationReviewAdmin(CMSAdmin):
    list_display = ('product', 'version', 'reviewed_by', 'reviewed_date', 'state')
    readonly_fields = ('customization', 'version', 'reviewed_date', 'reviewed_by',)

    list_filter = ('version__product__product_type', ProductFilter)

    change_form_template = 'cms/product_customization_review_change_form.html'
    fieldsets = (
        (None, {
            "fields": (
                'version', 'state', 'notes', 'reviewed_date', 'reviewed_by'
            )
        }),
    )

    def change_view(self, request, object_id, form_url='', extra_context=None):
        extra_context = extra_context or {}
        version = ProductCustomizationReview.objects.get(id=object_id).version
        extra_context['contexts'] = get_records_for_version(version)
        extra_context['title'] = "Changes for {}".format(version.product.name)
        if request.user == version.product.created_by:
            # extra_context['READONLY'] = True
            extra_context['review_states'] = ProductCustomizationReview.REVIEW_STATES
            extra_context['customization_reviews'] = version.productcustomizationreview_set.all()
        return super(ProductCustomizationReviewAdmin, self).change_view(
            request, object_id, form_url, extra_context=extra_context,
        )

    def get_queryset(self, request):
        qs = super(ProductCustomizationReviewAdmin, self).get_queryset(request)
        if not request.user.is_superuser:
            qs = qs.filter(customization__name=settings.CUSTOMIZATION)
        return qs

    def get_readonly_fields(self, request, obj=None):
        if request.user != obj.version.product.created_by:
            return self.readonly_fields
        return list(set(list(self.readonly_fields) +
                        [field.name for field in obj._meta.fields] +
                        [field.name for field in obj._meta.many_to_many]))

    def has_delete_permission(self, request, obj=None):
        if obj:
            return request.user == obj.version.product.created_by
        return False

    def product(self, obj):
        return obj.version.product

    def save_model(self, request, obj, form, change):
        # Once someone reviews the product we need to lock the version
        if not obj.version.accepted_date:
            obj.version.accepted_date = datetime.now()
            obj.version.save()
        obj.reviewed_by = request.user
        obj.reviewed_date = datetime.now()
        obj.save()


admin.site.register(ProductCustomizationReview, ProductCustomizationReviewAdmin)


class UserGroupsToCustomizationPermissionsAdmin(CMSAdmin):
    list_display = ('id', 'group', 'customization',)


admin.site.register(UserGroupsToCustomizationPermissions, UserGroupsToCustomizationPermissionsAdmin)
