from __future__ import unicode_literals

from django.db import models
from django.conf import settings
from jsonfield import JSONField


# CMS structure (data structure). Only developers can change that

class Product(models.Model):
    name = models.CharField(max_length=255, unique=True)


class Context(models.Model):
    product = models.ForeignKey(Product)
    name = models.CharField(max_length=1024)
    description = models.TextField()

    file_path = models.CharField(max_length=1024, blank=True, default='')
    url = models.CharField(max_length=1024, blank=True, default='')


DATA_TYPES = (
    (0, 'Text'),
    (1, 'Image'),
)


class DataStructure(models.Model):
    context = models.ForeignKey(Context)
    name = models.CharField(max_length=1024)
    description = models.TextField()

    type = models.IntegerField(choices=DATA_TYPES, default=0)
    default = models.CharField(max_length=1024, default='')
    # meta_settings = JSONField()


# CMS settings. Release engineer can change that

class Language(models.Model):
    name = models.CharField(max_length=255, unique=True)
    code = models.CharField(max_length=8, unique=True)


class Customization(models.Model):
    name = models.CharField(max_length=255, unique=True)
    default_language = models.ForeignKey(Language)


# CMS data. Partners can change that

class LanguageInCustomization(models.Model):
    language = models.ForeignKey(Language)
    customization = models.ForeignKey(Customization)


class ContentVersion(models.Model):
    customization = models.ForeignKey(Customization)
    name = models.CharField(max_length=1024)

    created_date = models.DateTimeField(auto_now_add=True)
    created_by = models.ForeignKey(settings.AUTH_USER_MODEL, null=True, blank=True, related_name='created_by_%(class)s')

    accepted_date = models.DateTimeField(null=True, blank=True)
    accepted_by = models.ForeignKey(settings.AUTH_USER_MODEL, null=True, blank=True,
                                    related_name='accepted_by_%(class)s')


class DataRecord(models.Model):
    data_structure = models.ForeignKey(DataStructure)
    language = models.ForeignKey(Language)
    customization = models.ForeignKey(Customization)
    version = models.ForeignKey(ContentVersion, null=True, blank=True)

    created_date = models.DateTimeField(auto_now_add=True)
    created_by = models.ForeignKey(settings.AUTH_USER_MODEL, null=True, blank=True, related_name='created_by_%(class)s')

    value = models.TextField()
