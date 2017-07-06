from __future__ import absolute_import
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated, AllowAny, IsAdminUser
from rest_framework.response import Response
from django.core.exceptions import PermissionDenied
from django.shortcuts import render, redirect
from cloud import settings

from .controllers.modify_db import *
from .forms import *

from django.views.generic.list import ListView

def get_post_parameters(request, context_id, language_id):
	context, language = get_context_and_language(request.data, context_id, language_id)
	customization = Customization.objects.get(name=settings.CUSTOMIZATION)		
	user = Account.objects.get(email=request.user)

	if not language and 'language' in request.session:
		languange = Language.objects.get(code=request.session['language'])

	if not language:
		language = customization.default_language
	
	form = CustomContextForm(initial={'language': language.id, 'context': context.id})

	return context, language, form, customization, user 


def handle_get_view(request, context_id, language_code):
	context, language, = None, None
	if context_id:
		context = Context.objects.get(id=context_id)
	
	if language_code:
		language = Language.objects.get(code=language_code)

	elif 'language' in request.session:
		language = Language.objects.get(code=request.session['language'])

	else:
		language = Customization.objects.get(name=settings.CUSTOMIZATION).default_language

	language_id = language.id

	form = CustomContextForm(initial={'language': language_id})
	if context:
		form.add_fields(context, language)

	return context, form, language
	

def handle_post_context_edit_view(request, context_id, language_id):
	context, language, form, customization, user = get_post_parameters(request, context_id, language_id)
	request_data = request.data
	preview_link = ""
	
	if 'languageChanged' in request_data:	
		if 'currentLanguage' in request_data and request_data['currentLanguage']:
			last_language = Language.objects.get(id=request_data['currentLanguage'])
			save_unrevisioned_records(customization, last_language, context.datastructure_set.all(), request_data, user)

	elif 'Preview' in request_data:
		save_unrevisioned_records(customization, language, context.datastructure_set.all(), request_data, user)
		preview_link = generate_preview(context)

	elif 'Publish' in request.data:
		publish_latest_version(user)

	elif 'SaveDraft' in request_data:
		save_unrevisioned_records(customization, language, context.datastructure_set.all(), request_data, user)

	elif 'SendReview' in request_data:
		send_version_for_review(customization, language, context.datastructure_set.all(), context.product, request_data, user)

	form.add_fields(context, language)

	return context, form, language, preview_link


# Create your views here.
@api_view(["GET", "POST"])
def context_edit_view(request, context=None, language=None):
	if request.method == "GET":
		context, form, language = handle_get_view(request, context, language)
		return render(request, 'context_editor.html', {'context': context,
													   'form': form,
													   'language': language})

	else:
		context, form, language, preview_link = handle_post_context_edit_view(request, context, language)
		return render(request, 'context_editor.html', {'context': context,
													   'form': form,
													   'language': language,
													   'preview_link': preview_link})


@api_view(["POST"])
def review_version_request(request, context=None, language=None):
	if "Preview" in request.data:
		preview_link = generate_preview()
		return redirect(preview_link)
	elif "Publish" in request.data:
		publish_latest_version(request.user)
		return Response("Published")
	return Response("Invalid")
		


@api_view(["GET"])
def review_version_view(request, version_id=None):
	version = ContentVersion.objects.get(id=version_id)
	data_records = version.datarecord_set.all().order_by('data_structure__context__name', 'language__code')

	
	contexts = {}
	for record in data_records:
		context_name = record.data_structure.context.name
		if  context_name in contexts:
			contexts[context_name].append(record)
		else:
			contexts[context_name] = [record]

	return render(request, 'review_records.html', {'version': version, 'contexts': contexts})
