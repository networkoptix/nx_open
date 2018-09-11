from django.conf import settings
from django.contrib import admin
from django.utils.html import format_html
import pytz

# Register your models here.

from .models import *
from .forms import *
from django_celery_results.models import TaskResult
admin.site.unregister(TaskResult)


class SubscriptionAdmin(admin.ModelAdmin):
    list_display = ('id', 'object', 'type', 'user_email', 'created_date', 'enabled')

    def has_delete_permission(self, request, obj=None):
        return False


admin.site.register(Subscription, SubscriptionAdmin)


class MessageAdmin(admin.ModelAdmin):
    list_display = ('type', 'user_email', 'created_date', 'send_date', 'delivery_time_interval', 'task_id')
    readonly_fields = ('user_email', 'external_id', 'task_id', 'type', 'customization',
                       'message', 'created_date', 'send_date', 'delivery_time_interval', 'event')
    list_filter = ('type', 'created_date', 'send_date')
    search_fields = ('user_email', 'created_date', 'send_date',)
    actions = ['clean_old_messages']

    def has_add_permission(self, request):  # No adding users in admin
        return False

    def has_delete_permission(self, request, obj=None):
        return False

    def clean_old_messages(self, request, queryset):
        from datetime import datetime, timedelta
        cutoff_date = datetime.now() - timedelta(days=settings.CLEAR_HISTORY_RECORDS_OLDER_THAN_X_DAYS)
        Message.objects.filter(send_date__lt=cutoff_date).delete()

    clean_old_messages.short_description = "Remove messages older than {} days"\
        .format(settings.CLEAR_HISTORY_RECORDS_OLDER_THAN_X_DAYS)


admin.site.register(Message, MessageAdmin)


class EventAdmin(admin.ModelAdmin):
    list_display = ('type', 'object', 'created_date', 'send_date', 'data')

    def has_delete_permission(self, request, obj=None):
        return False


admin.site.register(Event, EventAdmin)


class CloudNotificationAdmin(admin.ModelAdmin):
    list_display = ('subject', 'body', 'sent_by', 'convert_date')
    change_form_template = 'notifications/cloud_notifications_change_form.html'
    readonly_fields = ('sent_by', 'convert_date')
    form = CloudNotificationAdminForm
    fieldsets = [
        ("Subject and Body for email", {
            'fields': ('subject', 'body'),
            'description': "<div>Body should be formated in html</div>"
        }),
        ("When and who sent the notification", {'fields': (('sent_by', 'convert_date'))}),
        ("Target Customizations", {"fields": ("customizations",)})
    ]

    def get_form(self, request, obj=None, **kwargs):
        ModelForm = super(CloudNotificationAdmin, self).get_form(request, obj, **kwargs)

        class ModelFormMetaClass(ModelForm):
            def __new__(cls, *args, **kwargs):
                kwargs['user'] = request.user
                return ModelForm(*args, **kwargs)

        return ModelFormMetaClass

    def add_view(self, request, form_url='', extra_context=None):
        extra_context = extra_context or {}
        extra_context['BROADCAST_NOTIFICATIONS_SUPERUSERS_ONLY'] = settings.BROADCAST_NOTIFICATIONS_SUPERUSERS_ONLY
        return super(CloudNotificationAdmin, self).add_view(
            request, form_url, extra_context=extra_context,
        )

    def change_view(self, request, object_id, form_url='', extra_context=None):
        extra_context = extra_context or {}
        extra_context['BROADCAST_NOTIFICATIONS_SUPERUSERS_ONLY'] = settings.BROADCAST_NOTIFICATIONS_SUPERUSERS_ONLY
        return super(CloudNotificationAdmin, self).change_view(
            request, object_id, form_url, extra_context=extra_context,
        )

    def convert_date(self, obj):
        session = self.request.session
        if obj.sent_date and 'timezone' in self.request.session and self.request.session['timezone']:
            timezone = self.request.session['timezone']
            utc = pytz.utc.localize(obj.sent_date)
            converted_time = utc.astimezone(pytz.timezone(timezone))\
                                .replace(tzinfo=None).strftime("%b. %d, %Y, %H:%M")
            return format_html('<span title="{}">{}</span>'.format(timezone,converted_time))
        return obj.sent_date
    convert_date.short_description = "Sent date"
    convert_date.allow_tags = True
    convert_date.admin_order_field = "sent_date"

    def has_delete_permission(self, request, obj=None):
        self.request = request
        if obj and obj.sent_date:
            return False
        return super(CloudNotificationAdmin, self).has_delete_permission(request,obj=obj)

    def get_readonly_fields(self, request, obj=None):
        if obj and obj.sent_date:
            return self.readonly_fields + ('subject', 'body', 'customizations')
        return self.readonly_fields


admin.site.register(CloudNotification, CloudNotificationAdmin)


class TaskResultAdmin(admin.ModelAdmin):
    list_display = ('task_id', 'date_done', 'status')
    readonly_fields = ('date_done', 'result', 'hidden', 'meta')
    list_filter = ('date_done', 'status')
    search_fields = ('date_done', 'meta', 'result', 'task_id')
    actions = ['clean_old_tasks']
    fieldsets = (
        (None, {
            'fields': (
                'task_id',
                'status',
                'content_type',
                'content_encoding',
            ),
            'classes': ('extrapretty', 'wide')
        }),
        ('Result', {
            'fields': (
                'result',
                'date_done',
                'traceback',
                'hidden',
                'meta',
            ),
            'classes': ('extrapretty', 'wide')
        }),
    )

    class Meta:
        proxy = True

    def get_readonly_fields(self, request, obj=None):
        if request.user.is_superuser:
            return list(self.readonly_fields)
        return list(set(list(self.readonly_fields) +
                        [field.name for field in obj._meta.fields] +
                        [field.name for field in obj._meta.many_to_many]))

    def has_add_permission(self, request):  # No adding users in admin
        return False

    def has_delete_permission(self, request, obj=None):
        return False

    def clean_old_tasks(self, request, queryset):
        from datetime import datetime, timedelta
        cutoff_date = datetime.now() - timedelta(days=settings.CLEAR_HISTORY_RECORDS_OLDER_THAN_X_DAYS)
        TaskResult.objects.filter(date_done__lt=cutoff_date).delete()

    clean_old_tasks.short_description = "Remove tasks older than {} days"\
        .format(settings.CLEAR_HISTORY_RECORDS_OLDER_THAN_X_DAYS)


admin.site.register(TaskResult, TaskResultAdmin)
