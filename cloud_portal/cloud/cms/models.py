from __future__ import unicode_literals

from django.db import models
from django.conf import settings
from jsonfield import JSONField
from model_utils import Choices
from django.core.cache import cache
from util.config import get_config

from django.template.defaultfilters import truncatechars


def read_global_value(customization, value, version_id):
    from cms.models import Product
    product = Product.objects.get(name='cloud_portal')
    global_contexts = product.context_set.filter(is_global=True)
    record = None
    for context in global_contexts:
        records = context.datastructure_set.filter(name=value)
        if records.exists():
            record = records.last()
            break

    if not record:
        return None
    return find_actual_value(record, customization, version=version_id)


def find_actual_value(record, customization, language=None, version_id=None):
    # try to get translated content
    if language and record.translatable:
        content_record = DataRecord.objects \
            .filter(language_id=language.id,
                    data_structure_id=record.id,
                    customization_id=customization.id)

    # if not - get record without language
    if not content_record or not content_record.exists():
        content_record = DataRecord.objects \
            .filter(language_id=None,
                    data_structure_id=record.id,
                    customization_id=customization.id)

    # if not - get default language
    if not content_record or not content_record.exists():
        content_record = DataRecord.objects \
            .filter(language_id=customization.default_language_id,
                    data_structure_id=record.id,
                    customization_id=customization.id)

    if content_record and content_record.exists():
        if not version_id:
            content_value = content_record.latest('created_date').value
        else:  # Here find a datarecord with version_id
            # which is not more than version_id
            # filter only accepted content_records
            content_record = content_record.filter(
                version_id__lte=version_id)
            if content_record.exists():
                content_value = content_record.latest('version_id').value

    if not content_value:  # if no value - use default value from structure
        content_value = record.default

    return content_value


def customization_cache(customization_name, value=None):
    data = cache.get(customization_name)
    if not data:
        from cms.models import Customization
        customization = Customization.objects.get(name=customization_name)
        custom_config = get_config(customization)

        version_id = customization.contentversion_set.latest('accepted_date').id \
            if customization.contentversion_set.exists() else 0
        data = {
            'version_id': version_id,
            'languages': customization.languages.values_list('code', flat=True),
            'default_language': customization.default_language.code,
            'mail_from_name': read_global_value(customization, '%MAIL_FROM_NAME%', version_id),
            'mail_from_email': read_global_value(customization, '%MAIL_FROM_EMAIL%', version_id),
            'portal_url': custom_config['cloud_portal']['url']
        }
        cache.set(customization_name, data)

    if value:
        return data[value] if value in data else None
    return data


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

        version_id = customization_cache(self.customization.name, 'version_id')

        if version_id > self.id:
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
