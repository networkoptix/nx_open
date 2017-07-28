from django.contrib import admin
from models import *
# Register your models here.
from cloud import settings
from cms.admin import CMSAdmin


class AccountAdmin(CMSAdmin):
    list_display = ('email', 'first_name', 'last_name', 'created_date', 'last_login',
                    'subscribe', 'is_staff', 'language', 'customization')
    # forbid changing all fields which can be edited by user in cloud portal except sub
    readonly_fields = ('email', 'first_name', 'last_name', 'created_date', 'activated_date', 'last_login',
                       'subscribe', 'language', 'customization')

    list_filter = ('subscribe', 'is_staff', 'created_date', 'last_login',)
    search_fields = ('email', 'first_name', 'last_name', 'customization', 'language',)

    def save_model(self, request, obj, form, change):
        # forbid creating superusers outside specific domain
        if obj.is_superuser and not obj.email.endswith(settings.SUPERUSER_DOMAIN):
            obj.is_superuser = False

        # if this is superuser - make him is_staff too
        if obj.is_superuser:
            obj.is_staff = True

        obj.save()

    def get_queryset(self, request):  # show only users for current customization
        qs = super(AccountAdmin, self).get_queryset(request)  # Basic check from CMSAdmin
        if not request.user.is_superuser:  # only superuser can watch full accounts list
            qs = qs.filter(customization=settings.CUSTOMIZATION)
        return qs


admin.site.register(Account, AccountAdmin)
