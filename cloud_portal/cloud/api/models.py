from django.db import models
from django.contrib.auth.models import PermissionsMixin
from account_backend import AccountManager

from django.utils import timezone


class Account(PermissionsMixin):

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

    @staticmethod
    def is_authenticated():
        return True

    @staticmethod
    def is_anonymous():
        return False