from __future__ import unicode_literals
from django.contrib import admin, messages
from django.contrib.admin import SimpleListFilter
from django.db.models import Q
from django.shortcuts import redirect
from django.urls import reverse
from django.utils.html import format_html
from django.http.response import HttpResponse

from cloud import settings
from cms.forms import *
from cms.controllers.modify_db import get_records_for_version
from cms.views.product import page_editor, review


class CustomizationFilter(SimpleListFilter):
    title = 'Customization'
    parameter_name = 'customization'
    default_customization = None

    def lookups(self, request, model_admin):
        # Temporary customization 0 is need for 'All' since we need to keep it,
        # but choose the customization for the current cloud portal as the default value
        self.default_customization = Customization.objects.get(name=settings.CUSTOMIZATION).id
        customizations = [Customization(id=0, name='All')]
        customizations.extend(list(Customization.objects.filter(name__in=request.user.customizations)))
        return [(c.id, c.name) for c in customizations]

    def choices(self, cl):
        for lookup, title in self.lookup_choices:
            yield {
                'selected': self.value() == lookup if self.value() else lookup == self.default_customization,
                'query_string': cl.get_query_string({self.parameter_name: lookup}, []),
                'display': title,
            }

    def queryset(self, request, queryset):
        if self.value() and self.value() != '0':
            return queryset.filter(customization__id=self.value())

        if self.value() is None:
            return queryset.filter(customization__id=self.default_customization)
        return queryset


class ProductFilter(SimpleListFilter):
    title = 'Product'
    parameter_name = 'product'

    # TODO: show available products base on role/whats available
    def lookups(self, request, model_admin):
        products = Product.objects.all()
        if not request.user.is_superuser:
            products = products.filter(customizations__name__in=request.user.customizations)
        # TODO: Get list of available products for non context managers
        if not UserGroupsToProductPermissions.check_permission(request.user,
                                                               get_cloud_portal_product(settings.CUSTOMIZATION),
                                                               'cms.publish_version'):
            products = products.filter(created_by=request.user)

        return [(p.id, p.name) for p in products]

    def queryset(self, request, queryset):
        if self.value():
            return queryset.filter(version__product__id=self.value())
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
        if not UserGroupsToProductPermissions.check_permission(request.user,
                                                               get_cloud_portal_product(settings.CUSTOMIZATION)):
            # return empty dataset, only superuser can watch content in other
            # customizations
            return qs.filter(pk=-1)
        return qs


class ProductTypeAdmin(CMSAdmin):
    list_display = ('type', 'can_preview', 'single_customization',)


admin.site.register(ProductType, ProductTypeAdmin)


# TODO: CLOUD-2388  Add additional views to here link -> http://patrick.arminio.info/additional-admin-views/
class ProductAdmin(CMSAdmin):
    list_display = ('product_settings', 'edit_product', 'name', 'product_type', 'customizations_list', )
    list_display_links = ('name',)
    list_filter = ('product_type', )
    form = ProductForm
    change_form_template = 'cms/product_change_form.html'
    change_list_template = 'cms/product_changelist.html'

    def change_view(self, request, object_id, form_url='', extra_context=None):
        extra_context = extra_context or {}
        extra_context['current_versions'] = []
        approved = ProductCustomizationReview.REVIEW_STATES.accepted
        product = Product.objects.get(id=object_id)
        product_customizations = ProductCustomizationReview.objects.filter(version__product=product)

        for customization in product.customizations.filter(name__in=request.user.customizations):
            approved_versions = product_customizations.filter(customization=customization, state=approved)
            if approved_versions.exists():
                extra_context['current_versions'].append(approved_versions.latest('id'))
            else:
                extra_context['current_versions'].append({'customization': customization.name,
                                                          'id': "No Current Version"})

        return super(ProductAdmin, self).change_view(
            request, object_id, form_url, extra_context=extra_context,
        )

    def get_list_display(self, request):
        if not request.user.is_superuser:
            return self.list_display[1:]
        return self.list_display

    def get_queryset(self, request):
        queryset = super(ProductAdmin, self).get_queryset(request)
        if not request.user.is_superuser:
            viewable_products = [product.id for product in queryset
                                 if UserGroupsToProductPermissions.check_permission(request.user,
                                                                                    product,
                                                                                    'cms.can_access_product')]
            queryset = Product.objects.filter(id__in=viewable_products)
        return queryset

    def product_settings(self, obj):
        if obj.product_type and not obj.product_type.single_customization:
            return format_html('')
        return format_html('<a class="btn btn-sm" href="{}">Settings</a>',
                           reverse('product_settings', args=[obj.id]))

    product_settings.short_description = 'Product settings'
    product_settings.allow_tags = True

    def edit_product(self, obj):
        return format_html('<a class="btn btn-sm product" href="{}" value="{}">Edit contexts</a>',
                           reverse('admin:cms_contextproxy_changelist'), obj.id)

    edit_product.short_description = 'Edit page'
    edit_product.allow_tags = True

    def customizations_list(self, obj):
        return ", ".join(obj.customizations.values_list('name', flat=True))


admin.site.register(Product, ProductAdmin)


# TODO: CLOUD-2388  Remove this context admin. The product should use this logic for an additional view of the product
class ContextProxyAdmin(CMSAdmin):
    list_display = ('name', 'description', 'url', 'translatable', 'is_global')

    list_display_links = ('name',)
    search_fields = ('name', 'description', 'url')

    change_form_template = "cms/context_change_form.html"

    def change_view(self, request, object_id, form_url='', extra_context=None):
        extra_context = extra_context or {}
        if request.method == "POST" and 'product_id' in request.POST:
            extra_context['preview_link'] = page_editor(request)
            if 'SendReview' in request.POST:
                return redirect(extra_context['preview_link'].url)

        extra_context['title'] = "Edit {}".format(Context.objects.get(id=object_id).name)
        extra_context['language_code'] = Customization.objects.get(name=settings.CUSTOMIZATION).default_language
        extra_context['EXTERNAL_IMAGE'] = DataStructure.DATA_TYPES[
            DataStructure.DATA_TYPES.external_image]

        if 'admin_language' in request.session:
            extra_context['language_code'] = request.session['admin_language']
        if 'product_id' not in request.session:
            messages.error(request, "No product was selected!")
            return redirect(reverse('admin:cms_product_changelist'))
        extra_context['product'] = Product.objects.get(id=request.session['product_id'])

        form = CustomContextForm(initial={'language': extra_context['language_code'], 'context': object_id})
        form.add_fields(Product.objects.get(id=request.session['product_id']),
                        Context.objects.get(id=object_id),
                        Language.objects.get(code=request.session['language']),
                        request.user)
        extra_context['custom_form'] = form

        return super(ContextProxyAdmin, self).change_view(request, object_id, form_url, extra_context)

    def get_model_perms(self, request):
        if not request.user.is_superuser:
            return {}
        return super(ContextProxyAdmin, self).get_model_perms(request)

    def changelist_view(self, request, extra_context=None):
        extra_context = extra_context or {}
        if'product_id' in request.POST:
            request.session['product_id'] = request.POST['product_id']
            return HttpResponse(reverse('admin:cms_contextproxy_changelist'))

        if 'product_id' not in request.session:
            messages.error(request, "No product was selected!")
            return redirect(reverse('admin:cms_product_changelist'))

        return super(ContextProxyAdmin, self).changelist_view(request, extra_context)

    def get_queryset(self, request):  # show only users for cloud_portal product type
        qs = super(ContextProxyAdmin, self).get_queryset(request)  # Basic check from CMSAdmin
        qs = qs.filter(product_type__product__in=[request.session['product_id']])
        if not request.user.is_superuser:
            qs = qs.filter(hidden=False)  # only superuser sees hidden contexts
        return qs


admin.site.register(ContextProxy, ContextProxyAdmin)


class ContextAdmin(CMSAdmin):
    list_display = ('name', 'description', 'url', 'translatable', 'is_global')
    list_filter = ('product_type',)


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
    list_display = ('name', 'parent', 'trust_parent')
    form = CustomizationForm


admin.site.register(Customization, CustomizationAdmin)


class DataRecordAdmin(CMSAdmin):
    list_display = ('product', 'language', 'context',
                    'data_structure', 'short_description', 'version')
    list_filter = (ProductFilter, 'language', 'data_structure__context', 'data_structure')
    search_fields = ('data_structure__context__name', 'data_structure__name',
                     'data_structure__description', 'value', 'language__code')
    readonly_fields = ('created_by',)


admin.site.register(DataRecord, DataRecordAdmin)


class ContentVersionAdmin(CMSAdmin):
    list_display = ('id', 'product', 'created_date', 'created_by', 'state')

    list_display_links = ('id', )
    list_filter = (ProductFilter, CustomizationFilter,)
    search_fields = ('created_by__email',)
    readonly_fields = ('created_by',)
    exclude = ('accepted_by', 'accepted_date')

    def changelist_view(self, request, extra_context=None):
        if not request.user.is_superuser:
            self.list_display_links = (None,)
        return super(ContentVersionAdmin, self).changelist_view(request, extra_context)

    def get_queryset(self, request):  # show only users for current cloud_portal product
        qs = super(ContentVersionAdmin, self).get_queryset(request)  # Basic check from CMSAdmin
        if not request.user.is_superuser:
            qs = qs.filter(product__customizations__name__in=request.user.customizations)
        return qs


admin.site.register(ContentVersion, ContentVersionAdmin)


class ProductCustomizationReviewAdmin(CMSAdmin):
    list_display = ('product', 'version', 'customization', 'reviewed_by', 'reviewed_date', 'state')
    readonly_fields = ('customization', 'version', 'reviewed_date', 'reviewed_by', 'notes',)

    list_filter = ('version__product__product_type', ProductFilter, CustomizationFilter)

    change_form_template = 'cms/product_customization_review_change_form.html'
    fieldsets = (
        (None, {
            "fields": (
                'notes',
            )
        }),
    )

    def change_view(self, request, object_id, form_url='', extra_context=None):
        extra_context = extra_context or {}
        version = ProductCustomizationReview.objects.get(id=object_id).version
        extra_context['contexts'] = get_records_for_version(version)
        extra_context['title'] = "Changes for {} - Version: {}".format(version.product.name, version.id)

        extra_context['review_states'] = ProductCustomizationReview.REVIEW_STATES
        extra_context['customization_reviews'] = version.productcustomizationreview_set.\
            filter(customization__name__in=request.user.customizations)

        extra_context['DataStructureTypes'] = DataStructure.DATA_TYPES

        if request.user == version.product.created_by:
            extra_context['customization_reviews'] = version.productcustomizationreview_set.all()

        return super(ProductCustomizationReviewAdmin, self).change_view(
            request, object_id, form_url, extra_context=extra_context,
        )

    # TODO: filter visible reviews
    def get_queryset(self, request):
        qs = super(ProductCustomizationReviewAdmin, self).get_queryset(request)
        if not request.user.is_superuser:
            qs = qs.filter(Q(customization__name__in=request.user.customizations) or
                           Q(version__product__created_by=request.user))
        return qs

    def get_readonly_fields(self, request, obj=None):
        if request.user != obj.version.product.created_by and\
                obj.state != ProductCustomizationReview.REVIEW_STATES.rejected:
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
        # Save the review notes
        super(ProductCustomizationReviewAdmin, self).save_model(request, obj, form, change)
        # handle the action the user chose in product.review
        review(request)

    def response_change(self, request, obj):
        return redirect(reverse('admin:cms_productcustomizationreview_change', args=(obj.id,)))


admin.site.register(ProductCustomizationReview, ProductCustomizationReviewAdmin)


class UserGroupsToProductPermissionsAdmin(admin.ModelAdmin):
    list_display = ('id', 'group', 'product',)
    list_filter = ('product', )


admin.site.register(UserGroupsToProductPermissions, UserGroupsToProductPermissionsAdmin)


class ExternalFileAdmin(CMSAdmin):
    list_display = ('id', 'file', 'size',)


admin.site.register(ExternalFile, ExternalFileAdmin)
