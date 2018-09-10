from django.contrib import admin
from .models import *
from django.contrib.auth.models import Group
from .forms import *
# Register your models here.
from cloud import settings
from cms.admin import CMSAdmin
from django_csv_exports.admin import CSVExportAdmin

from django.contrib.admin import SimpleListFilter


class GroupFilter(SimpleListFilter):
    title = 'Group'
    parameter_name = 'group'

    def lookups(self, request, model_admin):
        groups = Group.objects.all()
        if not request.user.is_superuser:
            groups = groups.filter(usergroupstocustomizationpermissions__customization__name=settings.CUSTOMIZATION)
        return [(g.id, g.name) for g in groups]

    def queryset(self, request, queryset):
        if self.value():
            return queryset.filter(groups=self.value())
        return queryset


@admin.register(Account)
class AccountAdmin(CMSAdmin, CSVExportAdmin):
    list_display = ('short_email', 'short_first_name', 'short_last_name', 'created_date', 'last_login',
                    'subscribe', 'is_staff', 'language', 'customization')
    # forbid changing all fields which can be edited by user in cloud portal except sub
    readonly_fields = ('email', 'first_name', 'last_name', 'created_date', 'activated_date', 'last_login',
                       'subscribe', 'language', 'customization')

    exclude = ("user_permissions",)

    list_filter = ('subscribe', 'is_staff', 'created_date', 'last_login', 'customization', GroupFilter, )
    search_fields = ('email', 'first_name', 'last_name', 'customization', 'language', 'groups__name')

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

    def get_search_results(self, request, queryset, search_term):
        queryset, use_distinct = super(AccountAdmin, self).get_search_results(request, queryset, search_term)
        queryset |= self.model.objects.filter(email__icontains=search_term.lower())
        return queryset, use_distinct

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

    def get_readonly_fields(self, request, obj=None):
        return list(set(list(self.readonly_fields) +
                        [field.name for field in obj._meta.fields] +
                        [field.name for field in obj._meta.many_to_many]))

    def has_add_permission(self, request):
        return False

    def has_delete_permission(self, request, obj=None):
        return False

    clean_old_records.short_description = "Remove messages older than {} days".format(
        settings.CLEAR_HISTORY_RECORDS_OLDER_THAN_X_DAYS)


admin.site.unregister(Group)


@admin.register(Group)
class GroupAdmin(admin.ModelAdmin):
    # Use our custom form.
    form = GroupAdminForm
    # Filter permissions horizontal as well.
    filter_horizontal = ['permissions']
