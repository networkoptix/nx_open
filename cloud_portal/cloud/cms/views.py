from __future__ import absolute_import
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated, AllowAny, IsAdminUser
from rest_framework.response import Response
from django.core.exceptions import PermissionDenied
from django.shortcuts import render
from cloud import settings

from .forms import *
from .controllers.filldata import fill_content
from .controllers.modify_db import *

from django.views.generic.list import ListView

def handle_get_view(context_name, language_code):
	context, language, context_id, language_id = None, None, 0, 0
	if context_name:
		context = Context.objects.get(name=context_name)
		context_id = context.id
	if language_code:
		language = Language.objects.get(code=language_code)
		language_id = language.id

	form = CustomContextForm(initial={'language': language_id, 'context': context_id})
	if context:
		form.add_fields(context, language)

	return form, True


def handle_post_view(request):
	request_data = request.data
	context, language = get_post_request_params(request_data)

	form = CustomContextForm(initial={'language': language.id, 'context': context.id})
	preview_link = ""
	post_made = False

	customization = Customization.objects.get(name=settings.CUSTOMIZATION)		
	user = Account.objects.get(email=request.user)
	
	if 'GetDataStructures' in request_data:
		post_made = True
		form.add_fields(context, language)

	elif 'Preview' in request_data:
		#if check_if_form_touched(request_data, 'Preview'):
		save_unrevisioned_records(customization, language, context.datastructure_set.all(), request_data, user)

		fill_content(customization_name=settings.CUSTOMIZATION)
		preview_link = context.url + "?preview"

	elif 'Publish' in request.data:
		accept_latest_draft(user)
		fill_content(customization_name=settings.CUSTOMIZATION, preview=False)

	elif 'SaveDraft' in request_data: # and check_if_form_touched(request_data, 'SaveDraft'):
		save_unrevisioned_records(customization, language, context.datastructure_set.all(), request_data, user)

	elif 'SendReview' in request_data: # and check_if_form_touched(request_data, 'Review'):
		old_versions = ContentVersion.objects.filter(accepted_date=None)

		if old_versions.exists():
			old_version = old_versions.latest('created_date')
			alter_records_version(Context.objects.all(), customization, old_version, None)
			old_version.delete()

		save_unrevisioned_records(customization, language, context.datastructure_set.all(), request_data, user)

		version = ContentVersion(customization=customization, name="N/A", created_by=user)
		version.save()

		alter_records_version(Context.objects.all(), customization, None, version)
		#TODO add notification need to make template for this
		#notify_version_ready()

	return form, post_made, preview_link

# Create your views here.
@api_view(["GET", "POST"])
def context_edit_view(request, context=None, language=None):
	if request.method == "GET":
		form, post_made = handle_get_view(context, language)
		return render(request, 'context_editor.html', {'form': form, 'post_made': post_made})

	else:
		form, post_made, preview_link = handle_post_view(request)
		return render(request, 'context_editor.html', {'form': form, 'post_made': post_made, 'preview_link': preview_link})


@api_view(["GET", "POST"])
def partner_review_view(request, context=None, language=None):
	if request.method == "GET":
		form, post_made = handle_get_view(context, language)
		return render(request, 'partner_version_review.html', {'form': form})

	else:
		form, post_made, preview_link = handle_post_view(request)
		return render(request, 'partner_version_review.html', {'form': form, 'preview_link': preview_link})


class VersionsViewer(ListView):
	def get_queryset(self):
		query = ContentVersion.objects.get(id=self.kwargs['id']).datarecord_set.all()
		return query.order_by('data_structure__context__name', 'language__code')

	template = "datarecord_list.html"

	fields = "__all__"
