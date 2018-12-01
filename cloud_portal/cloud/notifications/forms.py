from django import forms
from django.contrib.admin.widgets import FilteredSelectMultiple

from .models import CloudNotification
from cms.models import Customization, UserGroupsToProductPermissions


class CloudNotificationAdminForm(forms.ModelForm):
    class Meta:
        model = CloudNotification
        exclude = []

    customizations = forms.ModelMultipleChoiceField(
        queryset=Customization.objects.values_list('name', flat=True),
        required=False,
        widget=FilteredSelectMultiple('customizations', False)
    )

    def __init__(self, *args, **kwargs):
        # Do the normal form initialisation.
        self.user = kwargs.pop('user', None)
        super(CloudNotificationAdminForm, self).__init__(*args, **kwargs)  # 'send_cloud_notification'

        if self.instance.pk and not self.instance.sent_date:
            groups = self.user.groups.filter(permissions__codename__contains="send_cloud_notification")
            self.fields['customizations'].queryset = UserGroupsToProductPermissions.objects.filter(
                group__in=groups).values_list('customization__name', flat=True).distinct()
            self.initial['customizations'] = self.instance.customizations.values_list('name', flat=True)
