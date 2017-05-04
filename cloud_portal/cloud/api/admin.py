from django.contrib import admin
from models import *
# Register your models here.


class AccountAdmin(admin.ModelAdmin):
    pass

admin.site.register(Account, AccountAdmin)
