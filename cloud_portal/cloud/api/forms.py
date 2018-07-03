from django import forms
from django.contrib.auth import get_user_model
from django.contrib.admin.widgets import FilteredSelectMultiple
from django.contrib.auth.models import Group

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
        widget=FilteredSelectMultiple('users', False)
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

    def save_m2m(self):
        # Add the users to the Group.
        self.instance.user_set = self.cleaned_data['users']
        self.instance.customizations_set = self.cleaned_data['customizations']

        for customization in self.cleaned_data['customizations']:
            if not UserGroupsToCustomizationPermissions.objects.filter(group=self.instance, customization=customization).exists():
                UserGroupsToCustomizationPermissions(group=self.instance, customization=customization).save()

    def save(self, *args, **kwargs):
        # Default save
        instance = super(GroupAdminForm, self).save()
        # Save many-to-many data
        self.save_m2m()
        return instance
