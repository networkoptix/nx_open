import yaml
from django import forms
from .models import *

import jsonfield
from dal import autocomplete


def convert_meta_to_description(meta):
    meta_to_plain = {"format": "Format:  %s",
                     "height": "Height: %spx",
                     "height_le": "Height: not greater than %spx",
                     "height_ge": "Height: not less than %spx",
                     "width": "Width: %spx",
                     "width_le": "Width: not greater than %spx",
                     "width_ge": "Width: not less than %spx",
                     "size": "Size limit: %s MB",
                     }
    converted_msg = ""
    for k in meta:
        if k in meta_to_plain:
            value = meta[k]

            if isinstance(value, list):
                value = ", ".join(value)
            converted_msg += "<br>" + meta_to_plain[k] % value

    return converted_msg


def get_languages_list():
    def modify_default_language(language):
        is_default = ""
        if language[0] == default_language_code:
            is_default = " - default"
        return language[0], "{} - {}{}".format(language[0],language[1], is_default)
    customization = Customization.objects.get(name=settings.CUSTOMIZATION)
    default_language_code = customization.default_language.code
    return map(modify_default_language, customization.languages.values_list('code', 'name'))


class CustomContextForm(forms.Form):
    language = forms.ChoiceField(
        widget=forms.Select, label="Language")

    def __init__(self, *args, **kwargs):
        super(CustomContextForm, self).__init__(*args, **kwargs)  # 'send_cloud_notification'
        self.fields['language'].choices = get_languages_list()

    def remove_langauge(self):
        super(CustomContextForm, self)
        self.fields.pop('language')

    def add_fields(self, product, context, language, user):
        data_structures = context.datastructure_set.order_by('order').all()

        if len(data_structures) < 1:
            return

        if not context.translatable:
            self.remove_langauge()

        for data_structure in data_structures:
            ds_label = data_structure.label if data_structure.label else data_structure.name

            ds_description = data_structure.description

            if data_structure.meta_settings:
                ds_description += convert_meta_to_description(data_structure.meta_settings)

            if data_structure.type == DataStructure.DATA_TYPES.guid:
                ds_description += "<br>GUID format is '{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXX}' using hexdecimal characters (0-9, a-f, A-F)"

            ds_language = language
            if not data_structure.translatable:
                if context.translatable:
                    ds_description += "<br>This record is the same for every language."
                ds_language = None

            record_value = data_structure.find_actual_value(product, ds_language)

            widget_type = forms.TextInput(attrs={'size': 80, 'placeholder': data_structure.default})

            disabled = data_structure.advanced and not (user.is_superuser or user.has_perm('cms.edit_advanced'))

            if data_structure.type == DataStructure.DATA_TYPES.long_text:
                widget_type = forms.Textarea(attrs={'placeholder': data_structure.default})

            if data_structure.type == DataStructure.DATA_TYPES.html:
                widget_type = forms.Textarea(
                    attrs={'cols': 120, 'rows': 25, 'class': 'tinymce'})

            if data_structure.type == DataStructure.DATA_TYPES.image:
                if not record_value:
                    record_value = data_structure.default
                self.fields[data_structure.name] = forms.ImageField(label=ds_label,
                                                                    help_text=ds_description,
                                                                    initial=record_value,
                                                                    required=False,
                                                                    disabled=disabled)
                continue

            elif data_structure.type == DataStructure.DATA_TYPES.file:
                if not record_value:
                    record_value = data_structure.default
                self.fields[data_structure.name] = forms.FileField(label=ds_label,
                                                                   help_text=ds_description,
                                                                   initial=record_value,
                                                                   required=False,
                                                                   disabled=disabled)
                continue

            elif data_structure.type == DataStructure.DATA_TYPES.external_file:
                external_file_field = "{}_external_file".format(data_structure.name)
                external_label = "{} file".format(ds_label)
                external_file_record = DataRecord.objects.filter(value=record_value).latest('id').external_file
                external_file = external_file_record.file.url if external_file_record else ""
                self.fields[external_file_field] = forms.FileField(label=external_label,
                                                                   help_text=ds_description,
                                                                   initial=external_file,
                                                                   required=False,
                                                                   disabled=disabled)

                ds_label = "{} meta data".format(ds_label)
                self.fields[data_structure.name] = jsonfield.fields.JSONCharFormField(required=False,
                                                                                      label=ds_label,
                                                                                      help_text=ds_description,
                                                                                      initial=record_value,
                                                                                      disabled=disabled)
                continue

            elif data_structure.type == DataStructure.DATA_TYPES.select:
                queryset = data_structure.meta_settings['options'] if 'options' in data_structure.meta_settings else []
                queryset = [(choice, choice) for choice in queryset]
                if 'multi' in data_structure.meta_settings and data_structure.meta_settings['multi']:
                    record_value = yaml.safe_load(record_value)
                    self.initial[data_structure.name] = record_value
                    self.fields[data_structure.name] = forms.MultipleChoiceField(label=ds_label,
                                                                                 help_text=ds_description,
                                                                                 choices=queryset,
                                                                                 required=False,
                                                                                 disabled=disabled)
                else:
                    self.fields[data_structure.name] = forms.ChoiceField(label=ds_label,
                                                                         help_text=ds_description,
                                                                         initial=record_value,
                                                                         choices=queryset,
                                                                         required=False,
                                                                         disabled=disabled)
                continue

            self.fields[data_structure.name] = forms.CharField(required=False,
                                                               label=ds_label,
                                                               help_text=ds_description,
                                                               initial=record_value,
                                                               widget=widget_type,
                                                               disabled=disabled)


class ProductSettingsForm(forms.Form):
    file = forms.FileField(
        label="File",
        help_text="Archive with static files and images for content or stucture.json file. "
                  "Archive must have top-level directories named as customizations",
        required=True
    )

    action = forms.ChoiceField(
        widget=forms.RadioSelect,
        required=True,
        choices=(
            ('generate_json', 'Generate structure template based on archive'),
            ('update_structure',
             'Update CMS structure and default values based on archive with structure.json and customization template'),
            ('update_content', 'Upload customized content files for customizations')
        )
    )


class ProductForm(forms.ModelForm):
    class Meta:
        model = Product
        exclude = []
        widgets = {
            'created_by': autocomplete.ModelSelect2(url='account-autocomplete',
                                                            attrs={
                                                                # Set some placeholder
                                                                'data-placeholder': 'Email ...',
                                                                #  Only trigger autocompletion after 2 characters have been typed
                                                                'data-minimum-input-length': 2
                                                            })
        }

    def __init__(self, *args, **kwargs):
        # Do the normal form initialisation.
        super(ProductForm, self).__init__(*args, **kwargs)
        cloud_portal = ProductType.PRODUCT_TYPES.cloud_portal
        if self.instance.product_type and self.instance.product_type.single_customization:
            cloud_customization = self.instance.customizations.first()
            used_customizations = [product.customizations.first().name
                                   for product in Product.objects.filter(product_type__type=cloud_portal)
                                   if product.customizations.exists() and product.customizations.first() != cloud_customization]
            self.fields['customizations'].queryset = Customization.objects.exclude(name__in=used_customizations)
            self.initial['customizations'] = self.instance.customizations.all()

    def clean_customizations(self):
        data = self.cleaned_data['customizations']
        if self.instance.product_type and self.instance.product_type.single_customization and len(data) > 1:
            raise ValueError('Too many customizations selected')
        return data
