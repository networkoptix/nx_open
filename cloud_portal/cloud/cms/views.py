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

from django.views.generic.list import ListView

def get_post_request_params(request_data): 
	return Context.objects.get(id=request_data['context']), Language.objects.get(id=request_data['language'])


def accept_latest_draft(user):
	unaccepted_version = ContentVersion.objects.filter(accepted_date=None).latest('created_date')
	unaccepted_version.accepted_by = user
	unaccepted_version.accepted_date = datetime.now()
	unaccepted_version.save()


def notify_version_ready():
	super_users = Account.objects.filter(is_superuser)
	for user in super_users:
		send_email(user.email, "version_ready_to_publish","",settings.CUSTOMIZATION)


def update_data_records_for_context(customization, language, data_structures, request_data, user, version=None):
	for data_structure in data_structures:
		data_structure_name = data_structure.name
		latest_record = data_structure.datarecord_set
		
		latest_record_value = latest_record.latest('created_date').value if latest_record.exists() else ""

		
		if data_structure_name in request_data and \
		   request_data[data_structure_name] != latest_record_value:
			new_record = request_data[data_structure_name]
			
			record = DataRecord(data_structure=data_structure,
								language=language,
								customization=customization,
								value=new_record,
								version=version,
								created_by=user)
			record.save()


def update_unapproved_to_new_version(old_version, new_version, data_structures):
	for data_structure in data_structures:
		latest_record = data_structure.datarecord_set.filter(version=old_version)
		if len(latest_record):
			latest_record[0].version = new_version
			latest_record[0].save()

	old_version.delete()


def check_if_form_touched(data, key_for_delete):
	exclusion_list = [key_for_delete, 'language', 'context']

	for key in data:
		if key in exclusion_list:
			continue
		if data[key]:
			return True
	return False


def handle_get_view(request):
	request_data = request.query_params
	context, context_id, language_id = None, 0, 0
	if request_data:
		context = Context.objects.get(name=request_data['context'])
		context_id = context.id
		if request_data['language']:
			language_id = Language.objects.get(code=request_data['language']).id

	form = CustomForm(initial={'language': language_id, 'context': context_id})
	if context:
		form.add_fields(context)

	return form, True


def handle_post_view(request):
	request_data = request.data
	context, language = get_post_request_params(request_data)

	form = CustomForm(initial={'language': language.id, 'context': context.id})
	preview_link = ""
	post_made = False

	customization = Customization.objects.get(name=settings.CUSTOMIZATION)		
	user = Account.objects.get(email=request.user)
	
	if 'GetDataStructures' in request_data:
		post_made = True
		form.add_fields(context)

	elif 'Preview' in request_data:
		if check_if_form_touched(request_data, 'Preview'):
			update_data_records_for_context(customization, language, context.datastructure_set.all(), request_data, user)

		fill_content(customization_name=settings.CUSTOMIZATION)
		preview_link = context.url + "?preview"

	elif 'Publish' in request.data:
		accept_latest_draft(user)
		fill_content(customization_name=settings.CUSTOMIZATION, preview=False)

	elif 'SaveDraft' in request_data and check_if_form_touched(request_data, 'SaveDraft'):
		update_data_records_for_context(customization, language, context.datastructure_set.all(), request_data, user)

	elif 'SendReview' in request_data and check_if_form_touched(request_data, 'Review'):
		old_version = ContentVersion.objects.all()
		old_version = old_version.filter(accepted_date=None) if len(old_version) else None
		old_version = old_version.latest('created_date') if old_version else None

		version = ContentVersion(customization=customization, name="N/A", created_by=user)
		version.save()

		if old_version:
			update_unapproved_to_new_version(old_version, version, context.datastructure_set.all())		
		update_data_records_for_context(customization, language, context.datastructure_set.all(), request_data, user, version)
		#TODO add notification need to make template for this
		#notify_version_ready()

	return form, post_made, preview_link

# Create your views here.
@api_view(["GET", "POST"])
def page_edit_view(request):
	if request.method == "GET":
		form, post_made = handle_get_view(request)
		return render(request, 'page_edit_form.html', {'form': form, 'post_made': post_made})

	else:
		form, post_made, preview_link = handle_post_view(request)
		return render(request, 'page_edit_form.html', {'form': form, 'post_made': post_made, 'preview_link': preview_link})


@api_view(["GET", "POST"])
def partner_publish_view(request):
	if request.method == "GET":
		form, post_made = handle_get_view(request)
		return render(request, 'partner_publish.html', {'form': form})

	else:
		form, post_made, preview_link = handle_post_view(request)
		return render(request, 'partner_publish.html', {'form': form, 'preview_link': preview_link})


class VersionsViewer(ListView):
	queryset = DataRecord.objects.order_by('data_structure__context__name', '-version__id', 'language__code')
	template = "datarecord_list.html"

	fields = "__all__"
