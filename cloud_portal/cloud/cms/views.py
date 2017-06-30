from __future__ import absolute_import
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated, AllowAny, IsAdminUser
from rest_framework.response import Response
from django.core.exceptions import PermissionDenied
from django.shortcuts import render
from cloud import settings

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

	return form
	

def handle_post_context_edit_view(request):
	context, language, form, customization, user = get_post_parameters(request)
	request_data = request.data
	preview_link = ""
	post_made = False
	
	if 'GetDataStructures' in request_data:
		post_made = True
		form.add_fields(context, language)

	elif 'Preview' in request_data:
		preview_link = generate_preview(customization, language, context.datastructure_set.all(), request_data, user)

	elif 'Publish' in request.data:
		publish_latest_version(user)

	elif 'SaveDraft' in request_data:
		save_unrevisioned_records(customization, language, context.datastructure_set.all(), request_data, user)

	elif 'SendReview' in request_data:
		send_version_for_review(customization, language, context.datastructure_set.all(), request_data, user)

	return form, post_made, preview_link


def handle_post_partner_review(request):
	request_data = request.data
	preview_link = ""
	form = None

	if 'Preview' in request_data:
		context, language, form, customization, user = get_post_parameters(request)
		preview_link = generate_preview(customization, language, context.datastructure_set.all(), request_data, user)

	elif 'Publish' in request.data:
		publish_latest_version(user)

	return form, preview_link


# Create your views here.
@api_view(["GET", "POST"])
def context_edit_view(request, context=None, language=None):
	if request.method == "GET":
		form = handle_get_view(context, language)
		return render(request, 'context_editor.html', {'form': form, 'post_made': True})

	else:
		form, post_made, preview_link = handle_post_context_edit_view(request)
		return render(request, 'context_editor.html', {'form': form, 'post_made': post_made, 'preview_link': preview_link})


@api_view(["GET", "POST"])
def partner_review_view(request, context=None, language=None):
	if request.method == "GET":
		form = handle_get_view(context, language)
		return render(request, 'partner_version_review.html', {'form': form})

	else:
		form, preview_link = handle_post_partner_review(request)
		return render(request, 'partner_version_review.html', {'form': form, 'preview_link': preview_link})


class VersionsViewer(ListView):
	def get_queryset(self):
		query = ContentVersion.objects.get(id=self.kwargs['id']).datarecord_set.all()
		return query.order_by('data_structure__context__name', 'language__code')

	template = "datarecord_list.html"

	fields = "__all__"
