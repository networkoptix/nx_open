from django.contrib import admin
from models import *
# Register your models here.
from cloud import settings
from cloud.cms.admin import CMSAdmin


class AccountAdmin(CMSAdmin):
    list_display = ('email', 'first_name', 'last_name', 'created_date', 'activated_date', 'last_login',
                    'subscribe', 'is_staff', 'is_superuser', 'language')

admin.site.register(Account, AccountAdmin)
