from django.contrib import admin

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
    list_display = ('id', 'sent_by', 'sent_date', 'subject', 'body')
    change_form_template = 'notifications/cloud_notifications_change_form.html'
    readonly_fields = ('sent_by', 'sent_date')
    fieldsets = [
        ("Subject and Body for email", {
            'fields': ('subject', 'body'),
            'description': "<div>Body should be formated in html</div>"
        }),
        ("When and who sent the notification", {'fields': (('sent_by', 'sent_date'))})
    ]


    #Right now only superusers can add notifications
    def has_add_permission(self, request):
        return request.user.has_perm('notifications.send_cloud_notification')


    def has_delete_permission(self, request, obj=None):
        if not obj:
            return request.user.has_perm('notifications.send_cloud_notification')
        return request.user.has_perm('notifications.send_cloud_notification') and not obj.sent_date


    def get_readonly_fields(self, request, obj=None):
        if obj and obj.sent_date:
            return self.readonly_fields + ('subject', 'body')
        return self.readonly_fields

admin.site.register(CloudNotification, CloudNotificationAdmin)
