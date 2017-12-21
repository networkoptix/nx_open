from django.contrib import admin

# Register your models here.

from .models import Message, Event, Subscription, CloudNotification


class SubscriptionAdmin(admin.ModelAdmin):
    list_display = ('id', 'object', 'type', 'user_email', 'created_date', 'enabled')

admin.site.register(Subscription, SubscriptionAdmin)


class MessageAdmin(admin.ModelAdmin):
    list_display = ('type', 'user_email', 'created_date', 'send_date', 'event', 'task_id', 'message')

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
        return request.user.is_superuser


    def has_delete_permission(self, request, obj=None):
        return request.user.is_superuser


    def get_readonly_fields(self, request, obj=None):
        if obj and obj.sent_date:
            return self.readonly_fields + ('subject', 'body')
        return self.readonly_fields


    def change_view(self, request, object_id, form_url='', extra_context=None):
        cloud_notification = CloudNotification.objects.get(id=object_id)
        extra_context = extra_context or {}
        extra_context['cloud_notification'] = cloud_notification
        return super(CloudNotificationAdmin, self).change_view(
            request, object_id, form_url, extra_context=extra_context,
        )

admin.site.register(CloudNotification, CloudNotificationAdmin)
