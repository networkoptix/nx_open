from django.contrib import admin
from django.utils.html import format_html
import pytz
import datetime

# Register your models here.

from .models import Message, Event, Subscription, CloudNotification


class SubscriptionAdmin(admin.ModelAdmin):
    list_display = ('id', 'object', 'type', 'user_email', 'created_date', 'enabled')

admin.site.register(Subscription, SubscriptionAdmin)


class MessageAdmin(admin.ModelAdmin):
    list_display = ('type', 'user_email', 'created_date', 'send_date', 'delivery_time_interval', 'task_id')
    readonly_fields = ('user_email', 'external_id', 'task_id', 'type', 'customization',
                       'message', 'created_date', 'send_date', 'delivery_time_interval', 'event')
    list_filter = ('type', 'created_date', 'send_date')
    search_fields = ('user_email', 'created_date', 'send_date',)

admin.site.register(Message, MessageAdmin)


class EventAdmin(admin.ModelAdmin):
    list_display = ('type', 'object', 'created_date', 'send_date', 'data')

admin.site.register(Event, EventAdmin)


class CloudNotificationAdmin(admin.ModelAdmin):
    list_display = ('subject', 'body', 'sent_by', 'convert_date')
    change_form_template = 'notifications/cloud_notifications_change_form.html'
    readonly_fields = ('sent_by', 'convert_date')
    fieldsets = [
        ("Subject and Body for email", {
            'fields': ('subject', 'body'),
            'description': "<div>Body should be formated in html</div>"
        }),
        ("When and who sent the notification", {'fields': (('sent_by', 'convert_date'))})
    ]

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
            return self.readonly_fields + ('subject', 'body')
        return self.readonly_fields

admin.site.register(CloudNotification, CloudNotificationAdmin)
