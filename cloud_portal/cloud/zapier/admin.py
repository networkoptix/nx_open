from __future__ import unicode_literals
from django.contrib import admin
from models import *

class GeneratedRuleAdmin(admin.ModelAdmin):
    list_display = ("id", "email", "system_id", "times_used", "direction", "caption", "source")
    list_filter = ("email", "system_id", "direction")
    search_fields = ("email", "system_id", "direction", "caption", "source")

admin.site.register(GeneratedRule, GeneratedRuleAdmin)


class ZapHookAdmin(admin.ModelAdmin):
    list_display = ("id", "user", "event", "target")
    list_filter = ("user__email", "event")
    search_fields = ("user", "event")

admin.site.register(ZapHook, ZapHookAdmin)
