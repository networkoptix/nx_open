from django.contrib import admin

# Register your models here.

from .models import Message, Event, Subscription


class SubscriptionAdmin(admin.ModelAdmin):
    list_display = ('id', 'object', 'type', 'user_email', 'created_date', 'enabled')

admin.site.register(Subscription, SubscriptionAdmin)


class MessageAdmin(admin.ModelAdmin):
    list_display = ('type', 'user_email', 'created_date', 'send_date', 'event', 'task_id', 'message')

admin.site.register(Message, MessageAdmin)


class EventAdmin(admin.ModelAdmin):
    list_display = ('type', 'object', 'created_date', 'send_date', 'data')

admin.site.register(Event, EventAdmin)
