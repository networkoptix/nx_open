from django.contrib import admin
from django.contrib.admin import SimpleListFilter
from datetime import datetime

from cloud import settings
from cms.admin import CMSAdmin
from cms.controllers.modify_db import get_records_for_version
from cms.models import Product, ProductType
from integration.models import ProductCustomizationReview


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
            extra_context['READONLY'] = True
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
