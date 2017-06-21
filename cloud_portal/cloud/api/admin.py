from django.contrib import admin
from models import *
# Register your models here.


class AccountAdmin(admin.ModelAdmin):
    list_display = [f.name for f in Account._meta.get_fields()]

admin.site.register(Account, AccountAdmin)
