from django.contrib import admin
from models import *
# Register your models here.
from cloud import settings
from cms.admin import CMSAdmin
from django_csv_exports.admin import CSVExportAdmin


@admin.register(Account)
class AccountAdmin(CMSAdmin, CSVExportAdmin):
    list_display = ('short_email', 'short_first_name', 'short_last_name', 'created_date', 'last_login',
                    'subscribe', 'is_staff', 'language', 'customization')
    # forbid changing all fields which can be edited by user in cloud portal except sub
    readonly_fields = ('email', 'first_name', 'last_name', 'created_date', 'activated_date', 'last_login',
                       'subscribe', 'language', 'customization')

    list_filter = ('subscribe', 'is_staff', 'created_date', 'last_login',)
    search_fields = ('email', 'first_name', 'last_name', 'customization', 'language',)

    csv_fields = ('email', 'first_name', 'last_name', 'created_date', 'last_login',
                    'subscribe', 'is_staff', 'language', 'customization')

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

    def has_add_permission(self, request):  # No adding users in admin
        return False

    def has_delete_permission(self, request, obj=None):  # No deleting users at all
        return False


@admin.register(AccountLoginHistory)
class AccountLoginHistoryAdmin(admin.ModelAdmin):
    list_display = ('action', 'email', 'ip', 'date')
    list_filter = ('action', 'date')
    search_fields = ('email', 'ip', 'date')

    actions = ['clean_old_records']

    def clean_old_records(self, request, queryset):
        from datetime import datetime, timedelta
        cutoff_date = datetime.now() - timedelta(days=settings.CLEAR_HISTORY_RECORDS_OLDER_THAN_X_DAYS)
        AccountLoginHistory.objects.filter(date__lt=cutoff_date).delete()

    clean_old_records.short_description = "Remove messages older than {} days".format(
        settings.CLEAR_HISTORY_RECORDS_OLDER_THAN_X_DAYS)
