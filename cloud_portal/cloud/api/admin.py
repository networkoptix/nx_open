from django.contrib import admin
from models import *
# Register your models here.
from cloud import settings


class AccountAdmin(admin.ModelAdmin):
    list_display = ('email', 'first_name', 'last_name', 'created_date', 'activated_date', 'last_login',
                    'subscribe', 'is_staff', 'is_superuser', 'language')

    def get_queryset(self, request):
        qs = super(AccountAdmin, self).get_queryset(request)
        return qs.filter(customization=settings.CUSTOMIZATION)
    	

admin.site.register(Account, AccountAdmin)
