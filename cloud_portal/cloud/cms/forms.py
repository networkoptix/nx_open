from django import forms
from .models import *


class CustomContextForm(forms.Form):
    language = forms.ModelChoiceField(
        widget=forms.Select, label="Language", queryset=Language.objects.all())

    def remove_langauge(self):
        super(CustomContextForm, self)
        self.fields.pop('language')

    def add_fields(self, context, customization, language):
        data_structures = context.datastructure_set.all()

        if len(data_structures) < 1:
            return

        if not context.translatable:
            self.remove_langauge()

        for data_structure in data_structures:
            ds_name = data_structure.name

            ds_description = data_structure.description

            ds_language = language
            if not data_structure.translatable:
                if context.translatable:
                    ds_description += "<br>This record is the same for every language."
                ds_language = None

            latest_record = data_structure.datarecord_set.filter(
                customization=customization, language=ds_language)

            record_value = latest_record.latest('created_date').value\
                if latest_record.exists() else data_structure.default

            widget_type = forms.TextInput(attrs={'size': 80})

            if data_structure.type == DataStructure.get_type("Long Text"):
                widget_type = forms.Textarea

            if data_structure.type == DataStructure.get_type("HTML"):
                widget_type = forms.Textarea(
                    attrs={'cols': 120, 'rows': 25, 'class': 'tinymce'})

            if data_structure.type == DataStructure.get_type("Image"):
                self.fields[ds_name] = forms.ImageField(label=ds_name,
                                                        help_text=ds_description,
                                                        initial=record_value,
                                                        required=False)
                continue

            self.fields[ds_name] = forms.CharField(required=False,
                                                   label=ds_name,
                                                   help_text=ds_description,
                                                   initial=record_value,
                                                   widget=widget_type)
