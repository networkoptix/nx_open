from __future__ import unicode_literals

from django.db import models
from django.conf import settings
from jsonfield import JSONField
from model_utils import Choices

from django.template.defaultfilters import truncatechars
from django.core.cache import cache


# CMS structure (data structure). Only developers can change that

class Product(models.Model):
    name = models.CharField(max_length=255, unique=True)

    def __str__(self):
        return self.name


class Context(models.Model):

    class Meta:
        verbose_name = 'page'
        verbose_name_plural = 'pages'
        permissions = (
            ("edit_content", "Can edit content and send for review"),
        )

    product = models.ForeignKey(Product, null=True)
    name = models.CharField(max_length=1024)
    description = models.TextField()
    translatable = models.BooleanField(default=True)
    is_global = models.BooleanField(default=False)
    hidden = models.BooleanField(default=False)

    file_path = models.CharField(max_length=1024, blank=True, default='')
    url = models.CharField(max_length=1024, blank=True, default='')

    def __str__(self):
        return self.name


DATA_TYPES = (
    (0, 'Text'),
    (1, 'Image'),
    (2, 'HTML'),
    (3, 'Long Text')
)


class DataStructure(models.Model):
    class Meta:
        permissions = (
            ("edit_advanced", "Can edit advanced DataStructures"),
        )
    context = models.ForeignKey(Context)
    name = models.CharField(max_length=1024)
    description = models.TextField()
    label = models.CharField(max_length=1024)

    type = models.IntegerField(choices=DATA_TYPES, default=0)
    default = models.TextField(default='')
    translatable = models.BooleanField(default=True)
    meta_settings = JSONField(default=dict())
    advanced = models.BooleanField(default=False)

    def __str__(self):
        return self.name

    @staticmethod
    def get_type(name):
        return next((type[0] for type in DATA_TYPES if type[1] == name), 0)

# CMS settings. Release engineer can change that


class Language(models.Model):
    name = models.CharField(max_length=255, unique=True)
    code = models.CharField(max_length=8, unique=True)

    def __str__(self):
        return self.code


class Customization(models.Model):
    name = models.CharField(max_length=255, unique=True)
    default_language = models.ForeignKey(
        Language, related_name='default_in_%(class)s')
    languages = models.ManyToManyField(Language)

    PREVIEW_STATUS = Choices((0, 'draft', 'draft'), (1, 'review', 'review'))
    preview_status = models.IntegerField(choices=PREVIEW_STATUS, default=PREVIEW_STATUS.draft)

    def __str__(self):
        return self.name


# CMS data. Partners can change that

class ContentVersion(models.Model):

    class Meta:
        verbose_name = 'revision'
        verbose_name_plural = 'revisions'
        permissions = (
            ("publish_version", "Can publish content to production"),
        )

    customization = models.ForeignKey(Customization)
    name = models.CharField(max_length=1024)

    created_date = models.DateTimeField(auto_now_add=True)
    created_by = models.ForeignKey(
        settings.AUTH_USER_MODEL, null=True,
        blank=True, related_name='created_%(class)s')

    accepted_date = models.DateTimeField(null=True, blank=True)
    accepted_by = models.ForeignKey(
        settings.AUTH_USER_MODEL, null=True, blank=True,
        related_name='accepted_%(class)s')

    def __str__(self):
        return str(self.id)


    @property
    def state(self):
        if self.accepted_by == None:
            return 'in review'

        customization_cache = cache
        data = customization_cache.get(self.customization.name)

        if not data:
            latest_version = ContentVersion.objects.latest('accepted_date')
            version_id = latest_version.id if latest_version.exists() else None
            data = {'version_id': version_id}
            customization_cache.set(self.customization.name, data)
        elif data['version_id'] > self.id:
                return 'old'

        return 'current'


class DataRecord(models.Model):
    data_structure = models.ForeignKey(DataStructure)
    language = models.ForeignKey(Language, null=True, blank=True)
    customization = models.ForeignKey(Customization)
    version = models.ForeignKey(ContentVersion, null=True, blank=True)

    created_date = models.DateTimeField(auto_now_add=True)
    created_by = models.ForeignKey(
        settings.AUTH_USER_MODEL, null=True,
        blank=True, related_name='created_%(class)s')

    value = models.TextField()

    def __str__(self):
        return self.value

    # added for images base64 encoding makes the field really long
    @property
    def short_description(self):
        return truncatechars(self.value, 100)

    def save(self, *args, **kwargs):
        if not self.data_structure.translatable:
            self.language = None

        super(DataRecord, self).save(*args, **kwargs)
