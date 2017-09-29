from __future__ import unicode_literals

from django.db import models


class GeneratedRules(models.Model):
    caption = models.CharField(max_length=1024)
    source = models.CharField(max_length=1024, default="")
    system_id = models.CharField(max_length=1024)
    email = models.CharField(max_length=1024)
    direction = models.CharField(max_length=100)