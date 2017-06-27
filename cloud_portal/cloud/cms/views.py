from __future__ import absolute_import
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated, AllowAny, IsAdminUser
from rest_framework.response import Response
from django.core.exceptions import PermissionDenied
from django.shortcuts import render
from cloud import settings
from datetime import datetime

from .forms import *
from .controllers.filldata import fill_content
from api.models import Account
from notifications.tasks import send_email


def update_data_records_for_context(customization, language, data_structures, request_data, user, version=None):
	for data_structure in data_structures:
		data_structure_name = data_structure.name
		latest_record = data_structure.datarecord_set
		
		latest_record_value = latest_record.latest('created_date').value if latest_record.exists() else ""

		
		if data_structure_name in request_data and request_data[data_structure_name] != latest_record_value:
			new_record = request_data[data_structure_name]
			
			record = DataRecord(data_structure=data_structure,
								language=language,
								customization=customization,
								value=new_record,
								version=version,
								created_by=user)
			record.save()

def check_if_form_touched(data, key_for_delete):
	exclusion_list = [key_for_delete, 'language', 'context']

	for key in data:
		if key in exclusion_list:
			continue
		if data[key]:
			return True
	return False

# Create your views here.
@api_view(["GET", "POST"])
def page_edit_view(request):
	form = CustomForm()
	preview_link = ""
	post_made = False
	if request.method == "POST":
		request_data = request.data

		context_id = request_data['context']
		context = Context.objects.get(id=context_id)

		language_id = request_data['language']
		language = Language.objects.get(id=language_id)

		customization = Customization.objects.get(name=settings.CUSTOMIZATION)
		
		user = Account.objects.get(email=request.user)
		
		if 'GetDataStructures' in request_data:
			post_made = True
			form.add_fields(context)

		elif 'SaveDraft' in request_data and check_if_form_touched(request_data, 'SaveDraft'):
				update_data_records_for_context(customization, language, context.datastructure_set.all(), request_data, user)

		elif 'Preview' in request_data:
			version_id = ContentVersion.objects.latest('created_date').id

			fill_content(customization_name=settings.CUSTOMIZATION, version_id=version_id)
			preview_link = context.url + "?preview"

		elif 'SendReview' in request_data and check_if_form_touched(request_data, 'Review'):
			version = ContentVersion(customization=customization, name="N/A", created_by=user)
			version.save()
			
			update_data_records_for_context(customization, language, context.datastructure_set.all(), request_data, user, version)
			#TODO add notification need to make template for this
			'''super_users = Account.objects.filter(is_superuser)
			for user in super_users:
				send_email(user.email, "version_ready_to_publish","",settings.CUSTOMIZATION)'''

		elif 'Publish' in request.data:
			unaccepted_versions = ContentVersion.objects.filter(accepted_date=None)
			for version in unaccepted_versions:
				version.accepted_by = user;
				version.accepted_date = datetime.now()
				version.save()

			fill_content(customization_name=settings.CUSTOMIZATION, preview=False)


	return render(request, 'page_edit_form.html', {'form': form, 'post_made': post_made, 'preview_link': preview_link})
