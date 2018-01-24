from django.contrib import admin
from django.urls import reverse
from django.utils.html import format_html

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
    list_display = ('context_actions', 'id', 'sent_by', 'sent_date', 'subject', 'body')
    list_display_links = ('id')
    change_form_template = 'notifications/cloud_notifications_change_form.html'
    readonly_fields = ('sent_by', 'sent_date')
    fieldsets = [
        ("Subject and Body for email", {
            'fields': ('subject', 'body'),
            'description': "<div>Body should be formated in html</div>"
        }),
        ("When and who sent the notification", {'fields': (('sent_by', 'sent_date'))})
    ]

    def context_actions(self, obj):
        return format_html('<a class="button" href="{}">Open Notification</a>',
                           reverse('admin:notifications_cloudnotification_change', args=[obj.id]))

    context_actions.short_description = 'Edit cloud notification'
    context_actions.allow_tags = True


    def has_delete_permission(self, request, obj=None):
        if not obj:
            return request.user.has_perm('notifications.delete_cloudnotification')
        return request.user.has_perm('notifications.delete_cloudnotification') and not obj.sent_date


    def get_readonly_fields(self, request, obj=None):
        if obj and obj.sent_date:
            return self.readonly_fields + ('subject', 'body')
        return self.readonly_fields

admin.site.register(CloudNotification, CloudNotificationAdmin)
