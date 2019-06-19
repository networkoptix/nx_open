from __future__ import unicode_literals
from django.contrib import admin, messages
from django.contrib.admin import SimpleListFilter
from django.contrib.auth.models import Permission
from django.conf.urls import url
from django.db.models import Q, Case, When, Value, BooleanField
from django.shortcuts import render, redirect
from django.urls import reverse
from django.utils.html import format_html

from cloud import settings
from cms.forms import *
from cms.controllers.modify_db import get_records_for_version
from cms.views.product import page_editor, review


def clone_product(request, product_id):
    product = Product.objects.get(id=product_id)
    clone_name = product.name + ' - copy'
    created_by = request.user
    customizations = product.customizations.all()

    if Product.objects.filter(name=clone_name).first():
        messages.error(request, "Copy already exists")
        return None

    if product.product_type.type == ProductType.PRODUCT_TYPES.cloud_portal:
        messages.error(request, "Cannot clone cloud portal products")
        return None

    product.pk = product.id = None
    product.name = clone_name
    product.created_by = created_by
    product.save()

    old_product = Product.objects.get(id=product_id)
    product.customizations.set(customizations)

    data_structures = DataStructure.objects.filter(context__product_type=product.product_type)
    for ds in data_structures:
        datarecord = old_product.datarecord_set.filter(data_structure=ds).last()
        if datarecord:
            datarecord.pk = datarecord.id = None
            datarecord.product = product
            datarecord.version = None
            datarecord.save()

    if '_clone_copy_perms' in request.POST or not request.user.is_superuser:
        grouptoproducts = UserGroupsToProductPermissions.objects.filter(product=old_product)
        for relation in grouptoproducts:
            UserGroupsToProductPermissions.objects.create(group=relation.group, product=product)

    return product.id


class CustomizationFilter(SimpleListFilter):
    title = 'Customization'
    parameter_name = 'customization'
    default_customization = None
    ALL_CUSTOMIZATIONS = '0'
    OTHER_CUSTOMIZATIONS = '1000'

    def lookups(self, request, model_admin):
        # Temporary customization 0 is need for 'All' since we need to keep it,
        # but choose the customization for the current cloud portal as the default value
        self.default_customization = Customization.objects.get(name=settings.CUSTOMIZATION).id
        customizations = [Customization(id=self.ALL_CUSTOMIZATIONS, name='All Customizations')]
        customizations.extend(list(Customization.objects.filter(name__in=request.user.customizations)))
        customizations.extend([Customization(id=self.OTHER_CUSTOMIZATIONS, name='Other Customizations')])
        return [(c.id, c.name) for c in customizations]

    def choices(self, cl):
        for lookup, title in self.lookup_choices:
            yield {
                'selected': self.value() == lookup if self.value() else lookup == self.default_customization,
                'query_string': cl.get_query_string({self.parameter_name: lookup}, []),
                'display': title,
            }

    def queryset(self, request, queryset):
        field_names = [f.name for f in queryset.model._meta.get_fields()]
        if 'customizations' in field_names:
            field_name = 'customizations'
        else:
            field_name = 'customization'

        if self.value():
            if self.value() == self.OTHER_CUSTOMIZATIONS:
                return queryset.exclude(**{field_name + '__name__in': request.user.customizations})
            elif self.value() != self.ALL_CUSTOMIZATIONS:
                return queryset.filter(**{field_name + '__id': self.value()})
        else:
            return queryset.filter(**{field_name + '__id': self.default_customization})
        return queryset


class ProductFilter(SimpleListFilter):
    title = 'Product'
    parameter_name = 'product'

    # TODO: show available products base on role/whats available
    def lookups(self, request, model_admin):
        products = Product.objects.all()
        if not request.user.is_superuser:
            products = products.filter(customizations__name__in=request.user.customizations).distinct()
        # TODO: Get list of available products for non context managers
        if not UserGroupsToProductPermissions.\
                check_customization_permission(request.user, settings.CUSTOMIZATION, 'cms.publish_version'):
            editable_products = request.user.products_with_permission('cms.edit_content')
            products = Product.objects.filter(Q(id__in=editable_products))

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


class ProductTypeAdmin(CMSAdmin):
    list_display = ('name', 'type', 'can_preview', 'single_customization',)
    list_display_links = ('name', 'type')


admin.site.register(ProductType, ProductTypeAdmin)


class ProductAdmin(CMSAdmin):
    list_display = ('product_settings', 'edit_product_button', 'name', 'product_type', 'customizations_list', )
    list_display_links = ('name',)
    list_filter = ('product_type', CustomizationFilter,)
    search_fields = ('name', 'created_by__email',)
    form = ProductForm
    change_form_template = 'cms/product_change_form.html'

    def get_form(self, request, obj=None, change=False, **kwargs):
        ProductForm = super().get_form(request, obj, change, **kwargs)

        class ProductFormWithUser(ProductForm):
            def __new__(cls, *args, **kwargs):
                kwargs['user'] = request.user
                return ProductForm(*args, **kwargs)

        return ProductFormWithUser

    def has_change_permission(self, request, obj=None):
        return request.user.is_superuser or len(request.user.products) > 0

    def has_view_permission(self, request, obj=None):
        return request.user.is_superuser or len(request.user.products) > 0

    def has_add_permission(self, request):
        return super(CMSAdmin, self).has_add_permission(request)

    def save_model(self, request, obj, form, change):
        super().save_model(request, obj, form, change)
        if not change and not request.user.is_superuser:
            group = Group.objects.create(name=obj.name + ' Developer')
            permissions = Permission.objects.filter(
                codename__in=['edit_content', 'change_product', 'change_productcustomizationreview']
            )
            group.user_set.add(request.user)
            group.permissions.set(permissions)
            UserGroupsToProductPermissions.objects.create(product=obj, group=group)

    def change_view(self, request, object_id, form_url='', extra_context=None):
        extra_context = extra_context or {}
        extra_context['current_versions'] = []
        approved = ProductCustomizationReview.REVIEW_STATES.accepted
        product = Product.objects.get(id=object_id)
        product_customizations = ProductCustomizationReview.objects.filter(version__product=product)

        for customization in product.customizations.filter(name__in=request.user.customizations):
            approved_version = product_customizations.filter(customization=customization, state=approved).last()
            if approved_version:
                extra_context['current_versions'].append(approved_version)
            else:
                extra_context['current_versions'].append({'customization': customization.name,
                                                          'id': "Not published"})

        extra_context['related_groups'] = Group.objects.filter(
            usergroupstoproductpermissions__product=product
        ).prefetch_related('permissions')

        if product.product_type.type != ProductType.PRODUCT_TYPES.cloud_portal:
            extra_context['show_clone_asset'] = True

        return super(ProductAdmin, self).change_view(
            request, object_id, form_url, extra_context=extra_context,
        )

    def get_fields(self, request, obj=None):
        fields = [field for field in self.form.base_fields]
        if obj:
            fields.remove('publish_all_customizations')
            if not request.user.is_superuser and not obj.product_type.single_customization:
                fields.remove('customizations')
        else:
            fields.remove('preview_status')
        return fields

    def get_readonly_fields(self, request, obj=None):
        if obj and not request.user.is_superuser:
            readonly_fields = super().get_readonly_fields(request, obj)
            readonly_fields.remove('name')
            readonly_fields.remove('contact_email')
            return readonly_fields

        return self.readonly_fields

    def get_list_display(self, request):
        if not request.user.is_superuser:
            return self.list_display[1:3]
        return self.list_display

    def get_queryset(self, request):
        queryset = super(ProductAdmin, self).get_queryset(request)
        if not request.user.is_superuser:
            editable_products = request.user.products_with_permission('cms.edit_content')
            queryset = Product.objects.filter(Q(id__in=editable_products))
        return queryset

    def get_list_filter(self, request):
        list_display = self.get_list_display(request)
        if 'customizations_list' not in list_display:
            list_filter = list(self.list_filter)
            list_filter.remove(CustomizationFilter)
            return list_filter
        return self.list_filter

    def get_urls(self):
        urls = super(ProductAdmin, self).get_urls()
        my_urls = [
            url(r'^(?P<product_id>.+?)/pages/$', self.admin_site.admin_view(self.page_list_view), name='pages'),
            url(r'^(?P<product_id>.+?)/pages/(?P<context_id>.+?)/change/$',
                self.admin_site.admin_view(self.change_page),
                name='change_page')
        ]
        return my_urls + urls

    def response_change(self, request, obj):
        if '_clone' in request.POST:
            new_id = clone_product(request, obj.id)
            if new_id:
                return redirect(reverse('admin:cms_product_change', args=(new_id,)))
            else:
                return redirect('.')
        return super().response_change(request, obj)

    def product_settings(self, obj):
        if obj.product_type.type == ProductType.PRODUCT_TYPES.integration:
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

    @staticmethod
    def change_page(request, context_id=None, product_id=None):
        context = {'errors': []}
        if request.method == "POST" and 'product_id' in request.POST:
            context['preview_link'], context['errors'] = page_editor(request)
            if 'SendReview' in request.POST and context['preview_link']:
                return redirect(context['preview_link'].url)

        target_context = Context.objects.get(id=context_id)
        product = Product.objects.get(id=product_id)

        context['title'] = f"Edit {target_context.get_nice_name()}"
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

    @staticmethod
    def customizations_list(obj):
        return ", ".join(obj.customizations.values_list('name', flat=True))


admin.site.register(Product, ProductAdmin)


class ContextAdmin(CMSAdmin):
    list_display = ('name', 'description', 'url', 'translatable', 'is_global', 'hidden')
    list_filter = ('product_type', 'translatable', 'is_global', 'hidden')


admin.site.register(Context, ContextAdmin)


class ContextTemplateAdmin(CMSAdmin):
    list_display = ('context', 'language', 'skin')
    list_filter = ('context', 'language', 'skin')
    search_fields = ('context__name', 'context__file_path', 'language__code')


admin.site.register(ContextTemplate, ContextTemplateAdmin)


class DataStructureAdmin(CMSAdmin):
    list_display = ('context', 'label', 'name', 'description', 'translatable', 'type', 'deprecated')
    list_filter = ('context', 'translatable', 'context__product_type', 'deprecated')
    search_fields = ('context__name', 'name', 'description', 'type')
    actions = ['delete_selected']


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
    list_display = ('product', 'version', 'customization_name', 'reviewer_email', 'reviewed_date', 'state', 'current_version')
    readonly_fields = ('customization', 'version', 'reviewed_date', 'reviewed_by', 'notes',)

    list_filter = ('version__product__product_type', 'state', ProductFilter, CustomizationFilter)

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
        customization_review = ProductCustomizationReview.objects.get(id=object_id)
        version = customization_review.version
        extra_context['contexts'] = get_records_for_version(version.product,
                                                            version,
                                                            customization_review.customization)

        extra_context['title'] = f"Changes for {version.product.name} - Version: {version.id}"

        extra_context['review_states'] = ProductCustomizationReview.REVIEW_STATES
        if UserGroupsToProductPermissions.check_permission(request.user, version.product, 'cms.edit_content'):
            extra_context['customization_reviews'] = version.productcustomizationreview_set.all()
        else:
            extra_context['customization_reviews'] = version.productcustomizationreview_set.\
                filter(customization__name__in=request.user.customizations)

        extra_context['DataStructureTypes'] = DataStructure.DATA_TYPES

        extra_context['allowed'] = self.template_allowed(request, customization_review)

        # Customization name should be visible in notes heading if developer has access or user has access
        customization_name = customization_review.customization.name
        if UserGroupsToProductPermissions.check_customization_access(request.user, customization_name) or \
                UserGroupsToProductPermissions.check_customization_access(version.created_by, customization_name):
            extra_context['customization_name'] = customization_name

        return super(ProductCustomizationReviewAdmin, self).change_view(
            request, object_id, form_url, extra_context=extra_context,
        )

    def get_object(self, request, object_id, from_field=None):
        review = self.get_queryset(request).get(id=object_id)
        if not UserGroupsToProductPermissions.check_customization_access(request.user, review.customization.name):
            review.customization = None
        return review

    # TODO: filter visible reviews
    def get_queryset(self, request):
        qs = super(ProductCustomizationReviewAdmin, self).get_queryset(request)
        if not request.user.is_superuser:
            qs = qs.filter(Q(customization__name__in=request.user.customizations_with_permission('cms.publish_version')))

            editable_products = request.user.products_with_permission('cms.edit_content')
            qs = qs | ProductCustomizationReview.objects.filter(Q(version__product__id__in=editable_products))
        can_view = request.user.customizations
        qs = qs.annotate(show_customization=Case(When(customization__name__in=can_view, then=Value(True)),
                                                 default=Value(False),
                                                 output_field=BooleanField()))

        return qs

    def get_readonly_fields(self, request, obj=None):
        if request.user != obj.version.product.created_by and\
                obj.state != ProductCustomizationReview.REVIEW_STATES.rejected:
            return self.readonly_fields
        return list(set(list(self.readonly_fields) +
                        [field.name for field in obj._meta.fields] +
                        [field.name for field in obj._meta.many_to_many]))

    def has_delete_permission(self, request, obj=None):
        if request.user.is_superuser:
            return True
        elif obj:
            return request.user == obj.version.product.created_by
        return False

    @staticmethod
    def product(obj):
        return obj.version.product

    def save_model(self, request, obj, form, change):
        # Save the review notes
        super(ProductCustomizationReviewAdmin, self).save_model(request, obj, form, change)
        # handle the action the user chose in product.review
        review(request)

    def response_change(self, request, obj):
        return redirect(reverse('admin:cms_productcustomizationreview_change', args=(obj.id,)))

    def current_version(self, obj):
        return obj.version.product.version_id(obj.customization.name) == obj.version.id

    current_version.short_description = "Current Published Version"
    current_version.boolean = True

    def customization_name(self, obj):
        return obj.customization if obj.show_customization else "-"

    customization_name.short_description = "Customization"

    def reviewer_email(self, obj):
        return obj.reviewed_by if obj.show_customization else "-"

    reviewer_email.short_description = "Reviewed By"

    def template_allowed(self, request, customization_review):
        customization_name = customization_review.customization.name
        matching_portal = customization_name == settings.CUSTOMIZATION
        is_cloud_portal = customization_review.version.product.is_cloud_portal
        state = customization_review.state

        can_access_customization = UserGroupsToProductPermissions.check_customization_access(
            request.user, customization_name
        )
        can_force_update = UserGroupsToProductPermissions.check_customization_permission(
            request.user, customization_name, 'cms.force_update'
        )
        can_publish_or_accept = UserGroupsToProductPermissions.check_customization_permission(
            request.user, customization_name, 'cms.publish_version'
        )
        developer_access_customization = UserGroupsToProductPermissions.check_customization_permission(
            customization_review.version.created_by, customization_name, 'cms.publish_version')
        can_delete = self.has_delete_permission(request, customization_review)

        allowed = dict()
        allowed['force_update'] = \
            is_cloud_portal and state == ProductCustomizationReview.REVIEW_STATES.accepted and matching_portal \
            and can_force_update
        allowed['publish'] = \
            is_cloud_portal and state == ProductCustomizationReview.REVIEW_STATES.pending and matching_portal \
            and can_publish_or_accept
        allowed['accept'] = \
            not is_cloud_portal and state == ProductCustomizationReview.REVIEW_STATES.pending \
            and can_publish_or_accept
        allowed['question'] = \
            (state == ProductCustomizationReview.REVIEW_STATES.pending or
             state == ProductCustomizationReview.REVIEW_STATES.rejected) and can_access_customization
        allowed['delete'] = can_delete
        allowed['submit_row'] = True in allowed.values()

        allowed['access_customization_checkbox'] = not developer_access_customization

        return allowed


admin.site.register(ProductCustomizationReview, ProductCustomizationReviewAdmin)


class UserGroupsToProductPermissionsAdmin(admin.ModelAdmin):
    list_display = ('id', 'group', 'product',)
    list_filter = ('product', )


admin.site.register(UserGroupsToProductPermissions, UserGroupsToProductPermissionsAdmin)


class ExternalFileAdmin(CMSAdmin):
    list_display = ('id', 'file', 'size',)


admin.site.register(ExternalFile, ExternalFileAdmin)
