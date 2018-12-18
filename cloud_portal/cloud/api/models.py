from django.db import models
from django.contrib.auth.models import Group, PermissionsMixin
from django.utils.deprecation import CallableFalse, CallableTrue
from django.utils.html import format_html

from api.account_backend import AccountManager
from cms.models import Customization, Product, ProductType, UserGroupsToProductPermissions
from cloud.settings import CUSTOMIZATION


class Account(PermissionsMixin):
    class Meta:
        permissions = (
            ("can_view_release", "Can view releases and patches"),
            ('invite_users', 'Invite users'),
        )

    objects = AccountManager()

    email = models.CharField(unique=True, max_length=255)
    first_name = models.CharField(max_length=255)
    last_name = models.CharField(max_length=255)
    created_date = models.DateTimeField(auto_now_add=True)
    activated_date = models.DateTimeField(null=True, blank=True)
    last_login = models.DateTimeField(null=True, blank=True)
    subscribe = models.BooleanField(default=False)
    is_active = models.BooleanField(default=True, help_text="If false the account is disabled. <br> If user was invited to the cloud it will switch to true on its own when the user completes registration.")
    is_staff = models.BooleanField(default=False, help_text="If true then the user can view cloud admin.")
    language = models.CharField(max_length=7, blank=True)
    customization = models.CharField(max_length=255, null=True)

    USERNAME_FIELD = 'email'
    REQUIRED_FIELDS = ['registeredDate', 'createdDate']

    def get_full_name(self):
        return self.first_name + ' ' + self.last_name

    def get_short_name(self):
        return self.first_name

    def get_username(self):
        return self.email

    def __str__(self):
        return self.get_username()

    @property
    def is_authenticated(self):
        return CallableTrue

    @property
    def is_anonymous(self):
        return CallableFalse

    @property
    def permissions(self):
        if not UserGroupsToProductPermissions.check_customization_permission(self, CUSTOMIZATION):
            return []

        permissions = []
        for group in self.groups.all():
            permissions.extend([permission.codename for permission in group.permissions.all()])
        return list(set(permissions))

    @property
    def customizations(self):
        if self.is_superuser:
            return Customization.objects.all().values_list('name')
        cloud_portal_ids = UserGroupsToProductPermissions.objects.\
            filter(group__in=self.groups.all(), product__product_type__type=ProductType.PRODUCT_TYPES.cloud_portal).\
            values_list('product__id', flat=True)

        customizations = [self.customization]

        for cloud_id in cloud_portal_ids:
            product = Product.objects.get(id=cloud_id)
            if UserGroupsToProductPermissions.check_permission(self, product, 'cms.can_view_customization'):
                customizations.append(product.customizations.first().name)

        return list(set(customizations))

    def short_email(self):
        return format_html("<div class='truncate-email'><span>{}</span></div>", self.email)

    def short_first_name(self):
        return format_html("<div class='truncate-name'><span>{}</span></div>", self.first_name)

    def short_last_name(self):
        return format_html("<div class='truncate-name'><span>{}</span></div>", self.last_name)

    short_email.short_description = "email"
    short_first_name.short_description = "first name"
    short_last_name.short_description = "last name"


class AccountLoginHistory(models.Model):
    action = models.CharField(max_length=64)
    ip = models.GenericIPAddressField(null=True)
    email = models.CharField(max_length=256, null=True)
    date = models.DateTimeField(auto_now_add=True)

    class Meta:
        verbose_name = 'Authentication log record'
        verbose_name_plural = 'Authentication log'

    def __unicode__(self):
        return '{0} - {1} - {2}'.format(self.action, self.email, self.ip)


class ProxyGroup(Group):
    pass

    class Meta:
        proxy = True
        app_label = 'api'
        verbose_name = 'Group'
        verbose_name_plural = 'Groups'
