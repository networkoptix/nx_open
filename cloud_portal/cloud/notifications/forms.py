from django import forms
from django.contrib.admin.widgets import FilteredSelectMultiple

from notifications.models import CloudNotification
from cms.models import Customization

class CloudNotificationAdminForm(forms.ModelForm):
    class Meta:
        model = CloudNotification
        exclude = []

    customizations = forms.ModelMultipleChoiceField(
        queryset=Customization.objects.values_list('name', flat=True),
        required=False,
        widget=FilteredSelectMultiple('customizations', False)
    )
