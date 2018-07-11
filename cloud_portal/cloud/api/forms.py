from django import forms
from django.contrib.auth import get_user_model
from django.contrib.admin.widgets import FilteredSelectMultiple
from django.contrib.auth.models import Group
from dal import autocomplete

from cms.models import Customization, UserGroupsToCustomizationPermissions

User = get_user_model()


# Create ModelForm based on the Group model.
class GroupAdminForm(forms.ModelForm):
    class Meta:
        model = Group
        exclude = []

    # Add the users field.
    users = forms.ModelMultipleChoiceField(
        queryset=User.objects.all(),
        required=False,
        # Use the pretty 'filter_horizontal widget'.
        widget=autocomplete.ModelSelect2Multiple(url='account-autocomplete',
                                                 attrs={
                                                     # Set some placeholder
                                                     'data-placeholder': 'Email ...',
                                                     # Only trigger autocompletion after 2 characters have been typed
                                                     'data-minimum-input-length': 2
                                                 })
                                            )

    customizations = forms.ModelMultipleChoiceField(
        queryset=Customization.objects.all(),
        required=False,
        widget=FilteredSelectMultiple('customizations', False)
    )

    def __init__(self, *args, **kwargs):
        # Do the normal form initialisation.
        super(GroupAdminForm, self).__init__(*args, **kwargs)
        # If it is an existing group (saved objects have a pk).
        if self.instance.pk:
            # Populate the users field with the current Group users.
            self.fields['users'].initial = self.instance.user_set.all()
            self.fields['customizations'].initial = UserGroupsToCustomizationPermissions.objects.filter(group=self.instance).values_list(
                'customization', flat=True).distinct()

    def save_m2m(self):
        # Add the users to the Group.
        self.instance.user_set = self.cleaned_data['users']
        self.instance.customizations_set = self.cleaned_data['customizations']

        for customization in self.cleaned_data['customizations']:
            if not UserGroupsToCustomizationPermissions.objects.filter(group=self.instance, customization=customization).exists():
                UserGroupsToCustomizationPermissions(group=self.instance, customization=customization).save()

        remove_permissions = UserGroupsToCustomizationPermissions.objects.filter(group=self.instance)\
                                                                 .exclude(customization__in=self.cleaned_data['customizations'])
        for customization_group in remove_permissions:
            customization_group.delete()

    def save(self, *args, **kwargs):
        # Default save
        instance = super(GroupAdminForm, self).save()
        # Save many-to-many data
        self.save_m2m()
        return instance
