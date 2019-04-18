from django.db import models
from django.contrib.auth.models import AbstractBaseUser, Group, PermissionsMixin
from django.utils.html import format_html

from django.utils import timezone


from api.controllers.cloud_api import Account as cloud_api_account
from api.helpers.exceptions import APIRequestException, APIException, APILogicException, ErrorCodes
from cms.models import Customization, Product, ProductType, UserGroupsToProductPermissions
from cloud.settings import CUSTOMIZATION


class AccountManager(models.Manager):

    """Custom manager for Account."""
    def _create_user(self, email, password, **extra_fields):
        """Create and save an Account with the given email and password.
        :param str email: user email
        :param str password: user password
        :param bool is_staff: whether user staff or not
        :param bool is_superuser: whether user admin or not
        :return custom_user.models.Account user: user
        :raise ValueError: email is not set
        """
        if not email:
            raise APIRequestException('Email code is absent', ErrorCodes.wrong_parameters,
                                      error_data={'email': ['This field is required.']})
        email = email.lower()

        ip = extra_fields.pop("IP", "")
        first_name = extra_fields.pop("first_name")
        last_name = extra_fields.pop("last_name")
        code = extra_fields.pop("code", None)

        # this line will send request to cloud_db and raise an exception if fails:
        try:
            cloud_api_account.register(ip, email, password, first_name, last_name, code=code)
        except APIException as a:
            if a.error_code == ErrorCodes.account_exists and not AccountManager.is_email_in_portal(email):
                raise APILogicException('User is not in portal', ErrorCodes.portal_critical_error)
            else:
                raise
        user = self.model(email=email,
                          first_name=first_name,
                          last_name=last_name,
                          customization=CUSTOMIZATION,
                          **extra_fields)

        if code:
            user.activated_date = timezone.now()
        user.save(using=self._db)

        return user

    def create_user(self, email, password, **extra_fields):
        return self._create_user(email, password, **extra_fields)

    def create_superuser(self, email, password, **extra_fields):
        return self._create_user(email, password, **extra_fields)

    def register_cloud_invite_user(self, email, password, data):
        ip = data.pop("IP", "")
        first_name = data.pop("first_name")
        last_name = data.pop("last_name")

        cloud_api_account.register(ip, email, password, first_name, last_name)
        user = Account.objects.get(email=email)
        """
        When an account is created using cloud invites it is disabled because its registration
        is different from regular users. Once the user has registered their account it is set to
        active in this function.
        """
        user.is_active = True
        user.first_name = first_name
        user.last_name = last_name
        user.save()

        return user

    @staticmethod
    def is_email_in_portal(email):
        return Account.objects.filter(email=email.lower()).count() > 0

    @staticmethod
    def check_email_in_portal(email, check_email_exists):
        mail_exists = AccountManager.is_email_in_portal(email)
        if not mail_exists and check_email_exists:
            raise APILogicException('User is not in portal', ErrorCodes.not_found)
        if mail_exists and not check_email_exists:
            raise APILogicException('User already registered', ErrorCodes.account_exists)
        return True


class Account(AbstractBaseUser, PermissionsMixin):
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
    ip = models.CharField(max_length=255, null=True)
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
