from __future__ import unicode_literals
from django.contrib import admin
from django.contrib.admin import SimpleListFilter
from django.conf.urls import url
from django.db.models import Q
from django.shortcuts import render, redirect
from django.urls import reverse
from django.utils.html import format_html

from cloud import settings
from cms.forms import *
from cms.controllers.modify_db import get_records_for_version
from cms.views.product import page_editor, review


class CustomizationFilter(SimpleListFilter):
    title = 'Customization'
    parameter_name = 'customization'
    default_customization = None
    all_customizations = '0'
    other_customizations = '1000'

    def lookups(self, request, model_admin):
        # Temporary customization 0 is need for 'All' since we need to keep it,
        # but choose the customization for the current cloud portal as the default value
        self.default_customization = Customization.objects.get(name=settings.CUSTOMIZATION).id
        customizations = [Customization(id=self.all_customizations, name='All Customizations')]
        customizations.extend(list(Customization.objects.filter(name__in=request.user.customizations)))
        customizations.extend([Customization(id=self.other_customizations, name='Other Customizations')])
        return [(c.id, c.name) for c in customizations]

    def choices(self, cl):
        for lookup, title in self.lookup_choices:
            yield {
                'selected': self.value() == lookup if self.value() else lookup == self.default_customization,
                'query_string': cl.get_query_string({self.parameter_name: lookup}, []),
                'display': title,
            }

    def queryset(self, request, queryset):
        if self.value():
            if self.value() == self.other_customizations:
                return queryset.exclude(customization__name__in=request.user.customizations)
            elif self.value() != self.all_customizations:
                return queryset.filter(customization__id=self.value())

        else:
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
        if not UserGroupsToProductPermissions.\
                check_customization_permission(request.user, settings.CUSTOMIZATION, 'cms.publish_version'):
            products = products.filter(created_by=request.user)

        return [(p.id, p.__str__()) for p in products]

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
        if not UserGroupsToProductPermissions.check_customization_permission(request.user, settings.CUSTOMIZATION):
            # return empty dataset, only superuser can watch content in other
            # customizations
            return qs.filter(pk=-1)
        return qs


class ProductTypeAdmin(CMSAdmin):
    list_display = ('type', 'can_preview', 'single_customization',)


admin.site.register(ProductType, ProductTypeAdmin)


class ProductAdmin(CMSAdmin):
    list_display = ('product_settings', 'edit_product_button', 'name', 'product_type', 'customizations_list', )
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

    def get_fields(self, request, obj=None):
        if not request.user.is_superuser and not obj.product_type.single_customization:
            return [field for field in self.form.base_fields if field != 'customizations']
        return self.form.base_fields

    def get_list_display(self, request):
        if not request.user.is_superuser:
            return self.list_display[1:3]
        return self.list_display

    def get_queryset(self, request):
        queryset = super(ProductAdmin, self).get_queryset(request)
        if not request.user.is_superuser:
            viewable_products = [product.id for product in queryset
                                 if UserGroupsToProductPermissions.check_permission(request.user,
                                                                                    product,
                                                                                    'cms.can_access_product')
                                 or product.created_by == request.user]
            queryset = Product.objects.filter(id__in=viewable_products)
        return queryset

    def get_urls(self):
        urls = super(ProductAdmin, self).get_urls()
        my_urls = [
            url(r'^(?P<product_id>.+?)/pages/$', self.admin_site.admin_view(self.page_list_view), name='pages'),
            url(r'^(?P<product_id>.+?)/pages/(?P<context_id>.+?)/change/$',
                self.admin_site.admin_view(self.change_page),
                name='change_page')
        ]
        return my_urls + urls

    def product_settings(self, obj):
        if obj.product_type and not obj.product_type.single_customization:
            return format_html('')
        return format_html('<a class="btn btn-sm" href="{}">Settings</a>',
                           reverse('product_settings', args=[obj.id]))

    product_settings.short_description = 'Product settings'
    product_settings.allow_tags = True

    def page_list_view(self, request, product_id=None):
        context = {
            'title': 'Edit a page',
            'app_label': self.model._meta.app_label,
            'opts': self.model._meta
        }

        if product_id:
            context['product'] = Product.objects.get(id=product_id)
            qs = context['product'].product_type.context_set.all()
            if not request.user.is_superuser:
                qs = qs.filter(hidden=False)  # only superuser sees hidden contexts
            context['contexts'] = qs

        return render(request, 'cms/page_list_view.html', context)

    def change_page(self, request, context_id=None, product_id=None):
        context = {'errors': []}
        if request.method == "POST" and 'product_id' in request.POST:
            context['preview_link'], context['errors'] = page_editor(request)
            if 'SendReview' in request.POST and context['preview_link']:
                return redirect(context['preview_link'].url)

        target_context = Context.objects.get(id=context_id)
        product = Product.objects.get(id=product_id)

        context['title'] = "Edit {}".format(target_context.name)
        context['language_code'] = Customization.objects.get(name=settings.CUSTOMIZATION).default_language
        context['EXTERNAL_IMAGE'] = DataStructure.DATA_TYPES[
            DataStructure.DATA_TYPES.external_image]

        if 'admin_language' in request.session:
            context['language_code'] = request.session['admin_language']

        context['product'] = product
        context['app_label'] = target_context._meta.app_label
        context['opts'] = target_context._meta
        context['product_opts'] = product._meta
        context['original'] = target_context

        form = CustomContextForm(initial={'language': context['language_code'], 'context': context_id})
        form.add_fields(product, target_context, Language.objects.get(code=context['language_code']), request.user)
        form.cleaned_data = {}
        for field_error in context['errors']:
            form.add_error(field_error[0], field_error[1])
        context['custom_form'] = form

        return render(request, 'cms/context_change_form.html', context)

    def edit_product_button(self, obj):
        return format_html('<a class="btn btn-sm product" href="{}">Edit content</a>',
                           reverse('admin:pages', args=[obj.id]))

    edit_product_button.short_description = 'Edit page'
    edit_product_button.allow_tags = True

    def customizations_list(self, obj):
        return ", ".join(obj.customizations.values_list('name', flat=True))


admin.site.register(Product, ProductAdmin)


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

    def get_object(self, request, object_id, from_field=None):
        review = self.get_queryset(request).get(id=object_id)
        if not UserGroupsToProductPermissions.check_customization_permission(request.user,
                                                                             review.customization.name,
                                                                             'cms.can_view_customization'):
            review.notes = review.anon_notes(review.notes)
        return review

    # TODO: filter visible reviews
    def get_queryset(self, request):
        qs = super(ProductCustomizationReviewAdmin, self).get_queryset(request)
        if not request.user.is_superuser:
            qs = qs.filter(Q(customization__name__in=request.user.customizations) |
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
