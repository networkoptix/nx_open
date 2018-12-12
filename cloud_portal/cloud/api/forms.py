from django import forms
from django.core.validators import EmailValidator
from django.contrib import messages
from django.contrib.auth import get_user_model
from django.contrib.admin.widgets import FilteredSelectMultiple
from django.contrib.auth.models import Group
from dal import autocomplete

import base64
from api.account_backend import AccountBackend
from api.models import Account
from cms.models import Customization, Product, UserGroupsToProductPermissions
from notifications import notifications_api

User = get_user_model()


class AccountAdminForm(forms.ModelForm):
    class Meta:
        model = Account
        exclude = []
        widgets = {
            'groups': FilteredSelectMultiple('groups', False)
        }


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

    products = forms.ModelMultipleChoiceField(
        queryset=Product.objects.all(),
        required=False,
        widget=FilteredSelectMultiple('products', False)
    )

    def __init__(self, *args, **kwargs):
        # Do the normal form initialisation.
        super(GroupAdminForm, self).__init__(*args, **kwargs)
        # If it is an existing group (saved objects have a pk).
        if self.instance.pk:
            # Populate the users field with the current Group users.
            self.fields['users'].initial = self.instance.user_set.all()
            self.fields['products'].initial = UserGroupsToProductPermissions.objects.filter(group=self.instance).values_list(
                'product', flat=True).distinct()

    def save_m2m(self):
        # Add the users to the Group.
        self.instance.user_set = self.cleaned_data['users']
        self.instance.products_set = self.cleaned_data['products']

        for product in self.cleaned_data['products']:
            if not UserGroupsToProductPermissions.objects.filter(group=self.instance, product=product).exists():
                UserGroupsToProductPermissions(group=self.instance, product=product).save()

        remove_permissions = UserGroupsToProductPermissions.objects.filter(group=self.instance).\
            exclude(product__in=self.cleaned_data['products'])
        for product_group in remove_permissions:
            product_group.delete()

    def save(self, *args, **kwargs):
        # Default save
        instance = super(GroupAdminForm, self).save()
        # Save many-to-many data
        self.save_m2m()
        return instance


class UserInviteFrom(forms.Form):
    email = forms.CharField(max_length=100, validators=[EmailValidator()])
    customization = forms.ChoiceField(choices=[])
    message = forms.CharField(widget=forms.Textarea)

    def __init__(self, *args, **kwargs):
        self.user = kwargs.pop('user', None)
        super(UserInviteFrom, self).__init__(*args, **kwargs)
        if self.user:
            self.fields['customization'].choices = [(customization[0], customization[0]) for customization in self.user.customizations]

    def add_user(self, request):
        email = request.POST['email']
        customization = request.POST['customization']
        message = request.POST['message']
        if AccountBackend.is_email_in_portal(email):
            messages.error(request, "User already has a cloud account!")
            return Account.objects.get(email=email).id

        messages.success(request, "User has been invited to cloud.")
        language_code = Customization.objects.get(name=customization).default_language.code
        user = Account(email=email, customization=customization, language=language_code, is_active=False)
        user.save()
        # Password in the encoded email doesnt matter its just a place holder.
        encode_email = base64.b64encode("password:{}".format(email))
        notifications_api.send(email, 'cloud_invite', {"message": message, "code": encode_email}, customization)

        return user.id
