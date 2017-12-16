from django.db import models
from django.contrib.auth.models import PermissionsMixin
from account_backend import AccountManager
from django.utils.deprecation import CallableFalse, CallableTrue
from django.utils.html import format_html


from cms.models import Customization
from cloud import settings

from django.utils import timezone

#current hack until 17.1.2 when we have global classes
email_css = "overflow: hidden;text-overflow: ellipsis;white-space: nowrap; width:205px;"
name_css = "overflow: hidden;text-overflow: ellipsis;white-space: nowrap; width:95px;"


class Account(PermissionsMixin):
    class Meta:
        permissions = (
            ("can_view_release", "Can view releases and patches"),
        )

    objects = AccountManager()

    email = models.CharField(unique=True, max_length=255)
    first_name = models.CharField(max_length=255)
    last_name = models.CharField(max_length=255)
    created_date = models.DateTimeField(auto_now_add=True)
    activated_date = models.DateTimeField(null=True, blank=True)
    last_login = models.DateTimeField(null=True, blank=True)
    subscribe = models.BooleanField(default=False)
    is_active = models.BooleanField(default=True)
    is_staff = models.BooleanField(default=False)
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

    def short_email(self):
        return format_html("<div style='{}'><span>{}</span></div>", email_css, self.email)

    def short_first_name(self):
        return format_html("<div style='{}'><span>{}</span></div>", name_css, self.first_name)

    def short_last_name(self):
        return format_html("<div style='{}'><span>{}</span></div>", name_css, self.last_name)

    short_email.short_description = "email"
    short_first_name.short_description = "first name"
    short_last_name.short_description = "last name"