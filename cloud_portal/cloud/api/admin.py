from django.contrib import admin
from models import *
# Register your models here.


class AccountAdmin(admin.ModelAdmin):
    list_display = ('email', 'first_name', 'last_name', 'created_date', 'activated_date', 'last_login',
                    'subscribe', 'is_staff', 'is_superuser', 'language')

admin.site.register(Account, AccountAdmin)
