from __future__ import unicode_literals
from django.contrib import admin
from models import *

class GeneratedRulesAdmin(admin.ModelAdmin):
    list_display = ("id", "email", "system_id", "direction", "caption", "source")
    list_filter = ("email", "system_id", "direction")
    search_fields = ("email", "system_id", "direction", "caption", "source")

admin.site.register(GeneratedRules, GeneratedRulesAdmin)