from __future__ import unicode_literals
import os
import re
from datetime import datetime
from distutils.util import strtobool
from django.db import models
from django.conf import settings
from jsonfield import JSONField
from model_utils import Choices
from django.core.cache import cache, caches
from util.config import get_config

from django.contrib.auth.models import Group
from django.template.defaultfilters import truncatechars
from cloud.storage_backend import MediaStorage


def get_cloud_portal_product(customization=settings.CUSTOMIZATION):
    return Product.objects.get(customizations__name__in=[customization],
                               product_type__type=ProductType.PRODUCT_TYPES.cloud_portal)


def get_product_by_revision(version_id):
    return Product.objects.get(contentversion__in=[version_id])


def update_global_cache(customization, version_id):
    global_cache = caches['global']
    global_cache.set(customization, version_id)


def check_update_cache(customization, version_id):
    global_cache = caches['global']
    global_id = global_cache.get(customization)

    return version_id != global_id, global_id


def cloud_portal_customization_cache(customization_name, value=None, force=False):
    data = cache.get(customization_name)
    product = get_cloud_portal_product(customization_name)

    if data and 'version_id' in data and not force:
        force = check_update_cache(customization_name, data['version_id'])[0]

    if not data or force:
        customization = Customization.objects.get(name=customization_name)
        custom_config = get_config(customization.name)

        data = {
            'version_id': product.version_id(),
            'languages': customization.languages_list,
            'default_language': customization.default_language.code,
            'mail_from_name': product.read_global_value('%MAIL_FROM_NAME%'),
            'mail_from_email': product.read_global_value('%MAIL_FROM_EMAIL%'),
            'portal_url': custom_config['cloud_portal']['url'],
            'smtp_host': product.read_global_value('%SMTP_HOST%'),
            'smtp_port': product.read_global_value('%SMTP_PORT%'),
            'smtp_user': product.read_global_value('%SMTP_USER%'),
            'smtp_password': product.read_global_value('%SMTP_PASSWORD%'),
            'smtp_tls': product.read_global_value('%SMTP_TLS%')
        }
        cache.set(customization_name, data)
        update_global_cache(customization, data['version_id'])

    if value:
        return data[value] if value in data else None

    return data


def slugify(name, lowercase=False):
    if lowercase:
        name = name.lower()
    unsafe_chars = re.compile('[^a-z0-9-]', flags=re.IGNORECASE)
    return unsafe_chars.sub('-', name)


def rename_file(instance, filename):
    product_name = slugify(instance.product.name, True)
    structure_name = slugify(instance.data_structure.name, True)
    file_info = "{}-{}-{}".format(structure_name, instance.id, slugify(filename))
    return os.path.join(product_name, file_info, filename)


# CMS structure (data structure). Only developers can change that
class Language(models.Model):
    name = models.CharField(max_length=255, unique=True)
    code = models.CharField(max_length=8, unique=True)

    def __str__(self):
        return self.code

    @staticmethod
    def by_code(language_code, default_language=None):
        if language_code:
            language = Language.objects.filter(code=language_code)
            if language.exists():
                return language.first()
        return default_language


class Customization(models.Model):
    class Meta:
        # Used to allow a user to see the customization in list of customizations
        # Cloud portal(s) are now a product so customization is not necessary for giving access anymore
        permissions = (
            ('can_view_customization', 'Can view customization'),
        )
    name = models.CharField(max_length=255, unique=True)
    default_language = models.ForeignKey(
        Language, related_name='default_in_%(class)s')
    languages = models.ManyToManyField(Language)
    filter_horizontal = ('languages',)

    public_release_history = models.BooleanField(default=False,
                                                 help_text="""Any user can view the release history page.""")
    public_downloads = models.BooleanField(default=True, help_text="""Any user can view the downloads page.""")

    parent = models.ForeignKey('Customization', default=None, null=True, blank=True,
                               related_name='children_customizations',
                               help_text="""Parent is the customization that the current customization depends on.<br>
                               The main purpose is to control how the integration review process works.
                               <br><br>
                               If there is a parent:<br>
                               - A review will be locked until the parent accepts that review.<br>
                               - If the parent rejects a review it will automatically be rejected for this
                               customization.<br><br>
                               If there is no parent selected or the parent is not in the review an integration
                               can be reviewed whenever.""")
    trust_parent = models.BooleanField(default=False, help_text="""Automatically accepts integrations the parent
                                                                   customization accepts.""")

    def __str__(self):
        return self.name

    @property
    def languages_list(self):
        return self.languages.values_list('code', flat=True)

    def get_children_ids(self, customization):
        children_list = []
        for child in customization.children_customizations.all():
            children_list.append(child.id)
            if child.children_customizations.exists():
                children_list.extend(self.get_children_ids(child))
        return children_list


class ProductType(models.Model):
    PRODUCT_TYPES = Choices((0, "cloud_portal", "Cloud Portal"),
                            (1, "vms", "Vms"),
                            (2, "plugin", "Plugin"),
                            (3, "integration", "Integration"))
    can_preview = models.BooleanField(default=False)
    single_customization = models.BooleanField(default=False)
    type = models.IntegerField(choices=PRODUCT_TYPES, default=PRODUCT_TYPES.cloud_portal)

    def __str__(self):
        return ProductType.PRODUCT_TYPES[self.type]

    @staticmethod
    def get_type_by_name(name):
        if name == "":
            return ProductType.PRODUCT_TYPES.cloud_portal
        if name[0].islower():
            return getattr(ProductType.PRODUCT_TYPES, name, ProductType.PRODUCT_TYPES.cloud_portal)

        for index, _name in ProductType.PRODUCT_TYPES:
            if _name == name:
                return index
        return 0


class Product(models.Model):
    class Meta:
        # The can_access_product gives users the ability to see the product in product lists.
        # In combination with other permission it allows them to edit the product and send reviews for their product
        permissions = (
            ('can_access_product', 'Can access product'),
        )
    name = models.CharField(max_length=255)
    created_by = models.ForeignKey(
        settings.AUTH_USER_MODEL, null=True,
        blank=True, related_name='created_%(class)s')
    customizations = models.ManyToManyField(Customization, default=None, blank=True)
    product_type = models.ForeignKey(ProductType, default=None, null=True)

    PREVIEW_STATUS = Choices((0, 'draft', 'draft'), (1, 'review', 'review'))
    preview_status = models.IntegerField(choices=PREVIEW_STATUS, default=PREVIEW_STATUS.draft)

    def __str__(self):
        if self.product_type and self.product_type.type == ProductType.PRODUCT_TYPES.cloud_portal:
            return "{} - {}".format(self.name, self.customizations.first())
        return self.name

    @property
    def default_language(self):
        if len(self.customizations.all()) == 1:
            return self.customizations.first().default_language

        return Customization.objects.get(name=settings.CUSTOMIZATION).default_language

    @property
    def languages_list(self):
        if self.customizations.exists():
            lang_list = []
            for customization in self.customizations.all():
                lang_list.extend(customization.languages_list)
            return list(set(lang_list))
        return Customization.objects.get(name=settings.CUSTOMIZATION).languages_list

    @property
    def product_root(self):
        if self.product_type and self.product_type.type == ProductType.PRODUCT_TYPES.cloud_portal:
            return self.customizations.first().name
        return ""

    def change_preview_status(self, new_status):
        self.preview_status = new_status
        self.save()

    def read_global_value(self, record_name):
        global_contexts = self.product_type.context_set.filter(is_global=True)
        data_structure = None
        for context in global_contexts:
            data_structures = context.datastructure_set.filter(name=record_name)
            if data_structures.exists():
                data_structure = data_structures.last()
                break

        if not data_structure:
            return None

        return data_structure.find_actual_value(product=self, version_id=self.version_id())

    def save(self, *args, **kwargs):
        need_update = False
        if self.pk is None:
            need_update = True
        else:
            orig = Product.objects.get(pk=self.pk)
            if self.customizations.exists():
                need_update = self.preview_status == orig.preview_status

        super(Product, self).save(*args, **kwargs)
        if need_update and self.product_type.type == ProductType.PRODUCT_TYPES.cloud_portal\
                and len(self.customizations.all()) == 1:
            cloud_portal_customization_cache(self.customizations.first().name, force=True)  # invalidate cache
            # TODO: need to update all static right here

    def version_id(self):
        # I need this for local dev until cloud-portal gets updated to 18.4
        # versions = ContentVersion.objects.filter(product=self)
        # return versions.latest('accepted_date').id if versions.exists() else 0
        accepted_review = ProductCustomizationReview.objects. \
            filter(customization__name=settings.CUSTOMIZATION,
                   state=ProductCustomizationReview.REVIEW_STATES.accepted,
                   version__product=self)

        return accepted_review.latest('id').version.id if accepted_review.exists() else 0


class Context(models.Model):
    class Meta:
        verbose_name = 'page'
        verbose_name_plural = 'pages'
        permissions = (
            ("edit_content", "Can edit content and send for review"),
        )
    # TODO: Remove this after release of 18.4 - Task: CLOUD-2299
    product = models.ForeignKey(Product, null=True)
    product_type = models.ForeignKey(ProductType, null=True)
    name = models.CharField(max_length=1024)
    description = models.TextField(blank=True, default="")
    translatable = models.BooleanField(default=True)
    is_global = models.BooleanField(default=False)
    hidden = models.BooleanField(default=False)

    file_path = models.CharField(max_length=1024, blank=True, default='')
    url = models.CharField(max_length=1024, blank=True, default='')

    def __str__(self):
        return self.name

    def template_for_language(self, language, default_language, skin):

        priorities = ((language, skin),  # exact match
                      (default_language, skin),  # skin is more important, fallback to default language
                      (None, skin),  # skin is more important, fallback to empty language
                      (language, ''),  # give up skin - find by lang only
                      (default_language, ''),  # fallback to default_language
                      (None, ''))  # default of default - no skin, no language

        # instantiate generator for contexts based on priorities
        contexts = (self.contexttemplate_set.filter(language=item[0], skin=item[1]) for item in priorities)

        # retrieve first available template from the list or return None
        return next((context_template.first().template for context_template in contexts if context_template.exists()),
                    None)


class ContextTemplate(models.Model):
    class Meta:
        unique_together = ('context', 'language', 'skin')

    context = models.ForeignKey(Context)
    language = models.ForeignKey(Language, null=True)
    template = models.TextField()
    skin = models.CharField(max_length=16, default=settings.DEFAULT_SKIN, blank=True)
    # Skin is a bit hacky for now:
    # Skin cannot be mentioned in filename
    # Skin is supported only for file contexts

    def __str__(self):
        if not self.language:
            return self.context.name
        if self.context.file_path:
            return ('{0}/'.format(self.skin) if self.skin else '') + \
                   self.context.file_path.replace("{{language}}", self.language.code)
        return "{0}-{1}{2}".format(self.context.name, '{0}/'.format(self.skin) if self.skin else '', self.language.name)


class DataStructure(models.Model):
    class Meta:
        permissions = (
            ("edit_advanced", "Can edit advanced DataStructures"),
        )
        index_together = [
            ["context", "order"],
        ]
    context = models.ForeignKey(Context)
    name = models.CharField(max_length=1024)
    description = models.TextField()
    label = models.CharField(max_length=1024, blank=True, default='')

    DATA_TYPES = Choices((0, 'text', 'Text'),
                         (1, 'image', 'Image'),
                         (2, 'html', 'HTML'),
                         (3, 'long_text', 'Long Text'),
                         (4, 'file', 'File'),
                         (5, 'guid', 'GUID'),
                         (6, 'select', 'Select'),
                         (7, 'external_file', 'External File'),
                         (8, 'external_image', 'External Image'),
                         (9, 'check_box', 'Check Box'),
                         (10, 'object', 'Object'),
                         (11, 'array', 'Array'))

    type = models.IntegerField(choices=DATA_TYPES, default=DATA_TYPES.text)
    default = models.TextField(default='', blank=True)
    translatable = models.BooleanField(default=True)
    meta_settings = JSONField(default=dict(),
                              blank=True,
                              help_text="For the regex field \\ needs to be escaped with another '\\'")
    advanced = models.BooleanField(default=False)
    order = models.IntegerField(default=100000)
    optional = models.BooleanField(default=False)
    public = models.BooleanField(default=True)

    def __str__(self):
        return self.name

    @staticmethod
    def get_type_by_name(name):
        if name[0].islower():
            return getattr(DataStructure.DATA_TYPES, name, DataStructure.DATA_TYPES.text)

        for index, _name in DataStructure.DATA_TYPES:
            if _name == name:
                return index
        return 0

    def find_actual_value(self, product=None, language=None, version_id=None, draft=False):
        content_value = ""
        if not product:
            return self.default
        content_record = DataRecord.objects.filter(product=product, data_structure=self)
        if not draft:
            content_record = content_record.\
                exclude(version__productcustomizationreview__state=ProductCustomizationReview.REVIEW_STATES.rejected)

        # try to get translated content
        if self.translatable:
            if language:
                content_record = content_record.filter(language=language)
            elif product.product_type.type == ProductType.PRODUCT_TYPES.cloud_portal:
                content_record = content_record.filter(language=product.customizations.first().default_language)

        if content_record and content_record.exists():
            if not version_id:
                content_value = content_record.latest('created_date').value
            else:  # Here find a datarecord with version_id
                # which is not more than version_id
                # filter only accepted content_records
                content_record = content_record.filter(version_id__lte=version_id)
                if not draft:
                    new_review_records = content_record.\
                        filter(version__productcustomizationreview__state=ProductCustomizationReview.
                               REVIEW_STATES.accepted)
                    # If new versions or records dont exist use old vay of getting records
                    if new_review_records.exists():
                        content_record = new_review_records
                    else:
                        content_record = content_record.filter(version__accepted_by__isnull=False)
                if content_record.exists():
                    content_value = content_record.latest('version_id').value

        # if no value or optional and type file - use default value from structure
        if not content_value and (not self.optional or
                                  self.optional and self.type in [DataStructure.DATA_TYPES.file,
                                                                  DataStructure.DATA_TYPES.image,
                                                                  DataStructure.DATA_TYPES.external_file,
                                                                  DataStructure.DATA_TYPES.external_image,
                                                                  DataStructure.DATA_TYPES.check_box]):
            content_value = self.default

        if self.type == DataStructure.DATA_TYPES.check_box:
            content_value = strtobool(content_value) == 1

        return content_value


# CMS settings. Release engineer can change that
class UserGroupsToProductPermissions(models.Model):
    group = models.ForeignKey(Group)
    product = models.ForeignKey(Product, default=None, null=True)

    @staticmethod
    def check_permission(user, product, permission=None):
        if user.is_superuser:
            return True
        if permission and not user.has_perm(permission):
            return False

        groups = UserGroupsToProductPermissions.objects.filter(product=product,
                                                               group_id__in=user.groups.values_list('id', flat=True))
        if permission:
            codename = permission
            if permission.find('.') > -1:
                # need to remove app_label to get codename
                codename = permission[permission.find('.')+1:]
            groups = groups.filter(group__permissions__codename=codename)
        return groups.exists()

    @staticmethod
    def check_customization_permission(user, customization, permission=None):
        return UserGroupsToProductPermissions.\
            check_permission(user, get_cloud_portal_product(customization), permission)


# CMS data. Partners can change that

class ContentVersion(models.Model):

    class Meta:
        verbose_name = 'revision'
        verbose_name_plural = 'revisions'

    # TODO: Remove this after release of 18.4 - Task: CLOUD-2299
    customization = models.ForeignKey(Customization, default=None, null=True)
    product = models.ForeignKey(Product, default=1)
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

    def create_reviews(self):
        blocked = ProductCustomizationReview.REVIEW_STATES.blocked
        pending = ProductCustomizationReview.REVIEW_STATES.pending

        for customization in self.product.customizations.all():
            if customization.parent:
                ProductCustomizationReview(customization=customization, version=self, state=blocked).save()
            else:
                ProductCustomizationReview(customization=customization, version=self, state=pending).save()

    @property
    def state(self):
        if not self.accepted_by:
            return 'in review'

        version_id = self.product.version_id()

        if version_id > self.id:
            return 'old'

        return 'current'


class ProductCustomizationReview(models.Model):
    class Meta:
        verbose_name = 'product review'
        verbose_name_plural = 'product reviews'
        permissions = (
            ("publish_version", "Can publish content to production"),
            ("force_update", "Can forcibly update content"),
        )

    REVIEW_STATES = Choices((0, "pending", "Pending"),
                            (1, "accepted", "Accepted"),
                            (2, "rejected", "Rejected"),
                            (3, "blocked", "Blocked"))
    customization = models.ForeignKey(Customization)
    version = models.ForeignKey(ContentVersion)
    state = models.IntegerField(choices=REVIEW_STATES, default=REVIEW_STATES.pending)
    notes = models.TextField(default="", blank=True)
    reviewed_date = models.DateTimeField(null=True, blank=True)
    reviewed_by = models.ForeignKey(
        settings.AUTH_USER_MODEL, null=True, blank=True,
        related_name='accepted_%(class)s')

    def __str__(self):
        return self.version.product.__str__()

    def update_children_reviews(self):
        reviews = self.version.productcustomizationreview_set.\
            filter(customization__in=self.customization.children_customizations.all())

        for review in reviews:
            review.reviewed_by = self.reviewed_by
            review.reviewed_date = self.reviewed_date
            review.state = self.state

            if review.state == ProductCustomizationReview.REVIEW_STATES.accepted:
                if review.customization.trust_parent:
                    review.notes = "Automatically accepted by {}".format(self.customization)
                else:
                    review.state = ProductCustomizationReview.REVIEW_STATES.pending
                    # If the child customization does not trust its parent we need to set reviewed by and date to blank.
                    review.reviewed_by = None
                    review.reviewed_date = None
            else:
                review.notes = "Automatically rejected by {}".format(self.customization)

            review.save()
            if review.state == ProductCustomizationReview.REVIEW_STATES.rejected or review.customization.trust_parent:
                review.update_children_reviews()

    def update_state(self, user, state):
        self.reviewed_by = user
        self.reviewed_date = datetime.now()
        self.state = state
        self.save()
        self.update_children_reviews()

    @staticmethod
    def anon_notes(notes):
        if notes:
            for i, email in enumerate(list(set(re.findall('(.*@*):', notes)))):
                notes = notes.replace(email, 'User {}'.format(i+1))
        return notes


class ExternalFile(models.Model):
    data_structure = models.ForeignKey(DataStructure, default=None, null=True)
    file = models.FileField(upload_to=rename_file, storage=MediaStorage())
    md5 = models.CharField(max_length=1024, default='')
    product = models.ForeignKey(Product, default=None, null=True)
    size = models.FloatField(default=0.0)

    def __str__(self):
        return self.file.name


class DataRecord(models.Model):
    data_structure = models.ForeignKey(DataStructure)
    product = models.ForeignKey(Product, default=None, null=True)
    language = models.ForeignKey(Language, null=True, blank=True)
    # TODO: Remove this after release of 18.4 - Task: CLOUD-2299
    customization = models.ForeignKey(Customization, default=None, blank=True, null=True)
    version = models.ForeignKey(ContentVersion, null=True, blank=True)

    created_date = models.DateTimeField(auto_now_add=True)
    created_by = models.ForeignKey(
        settings.AUTH_USER_MODEL, null=True,
        blank=True, related_name='created_%(class)s')

    value = models.TextField(default='', blank=True)
    external_file = models.ForeignKey(ExternalFile, default=None, blank=True, null=True)

    def __str__(self):
        return self.value

    # added for images base64 encoding makes the field really long
    @property
    def short_description(self):
        return truncatechars(self.value, 100)

    @property
    def context(self):
        return self.data_structure.context

    def save(self, *args, **kwargs):
        if not self.data_structure.translatable:
            self.language = None

        super(DataRecord, self).save(*args, **kwargs)
