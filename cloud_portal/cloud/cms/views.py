from __future__ import absolute_import
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated, AllowAny, IsAdminUser
from rest_framework.response import Response

from django.contrib import messages
from django.core.exceptions import PermissionDenied
from django.shortcuts import render, redirect
from django.urls import reverse

from cloud import settings

from .controllers.modify_db import *
from .forms import *

from django.contrib.admin import AdminSite

class MyAdminSite(AdminSite):
            pass
mysite = MyAdminSite()

def get_post_parameters(request, context_id, language_id):
	context, language = get_context_and_language(request.data, context_id, language_id)
	customization = Customization.objects.get(name=settings.CUSTOMIZATION)		
	user = Account.objects.get(email=request.user)

	if not language and 'language' in request.session:
		languange = Language.objects.get(code=request.session['language'])

	if not language:
		language = customization.default_language
	
	form = CustomContextForm(request.POST, request.FILES, initial={'language': language.id, 'context': context.id})

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
	request_files = request.FILES
	preview_link = ""
	encoded_image = None
	
	if 'languageChanged' in request_data:	
		if 'currentLanguage' in request_data and request_data['currentLanguage']:
			last_language = Language.objects.get(id=request_data['currentLanguage'])
			save_unrevisioned_records(customization, last_language, context.datastructure_set.all(), request_data, request_files, user)
		messages.success(request._request, "Changes have been saved.")

	elif 'Preview' in request_data:
		save_unrevisioned_records(customization, language, context.datastructure_set.all(), request_data, request_files, user)
		preview_link = generate_preview(context)
		messages.success(request._request, "Changes have been saved. Preview has been created.")

	elif 'SaveDraft' in request_data:
		save_unrevisioned_records(customization, language, context.datastructure_set.all(), request_data, request_files, user)
		messages.success(request._request, "Changes have been saved.")

	elif 'SendReview' in request_data:
		send_version_for_review(customization, language, context.datastructure_set.all(), context.product, request_data, request_files, user)
		messages.success(request._request, "Changes have been saved. A new version has been created.")
		return None, None, None, None

	form.add_fields(context, language)

	return context, form, language, preview_link


# Create your views here.
@api_view(["GET", "POST"])
def context_edit_view(request, context=None, language=None):
	if request.method == "GET":
		context, form, language = handle_get_view(request, context, language)
		return render(request, 'context_editor.html', {'context': context,
													   'form': form,
													   'language': language,
													   'user': request.user,
													   'has_permission': mysite.has_permission(request),
													   'site_url': mysite.site_url,
													   'title': 'Content Editor'})

	else:
		context, encoded_image, form, language, preview_link = handle_post_context_edit_view(request, context, language)
		

		if 'SendReview' in request.data:
			return redirect(reverse('review_version', args=[ContentVersion.objects.latest('created_date').id]))

		return render(request, 'context_editor.html', {'context': context,
													   'form': form,
													   'language': language,
													   'preview_link': preview_link,
													   'user': request.user,
													   'has_permission': mysite.has_permission(request),
													   'site_url': mysite.site_url,
													   'title': 'Content Editor',
													   'image':encoded_image})


@api_view(["POST"])
def review_version_request(request, context=None, language=None):
	if "Preview" in request.data:
		preview_link = generate_preview()
		return redirect(preview_link)
	elif "Publish" in request.data:
		publish_latest_version(request.user)
		version = ContentVersion.objects.latest('created_date')
		contexts = get_records_for_version(version)
		messages.success(request._request, "Version " + str(version.id) +" has been published")
		return render(request, 'review_records.html', {'version': version.id,
													   'contexts': contexts,
													   'user': request.user,
													   'has_permission': mysite.has_permission(request),
													   'site_url': mysite.site_url,
													   'title': 'Review A version'})
	return Response("Invalid")
		


@api_view(["GET"])
def review_version_view(request, version_id=None):
	version = ContentVersion.objects.get(id=version_id)
	contexts = get_records_for_version(version)
	return render(request, 'review_records.html', {'version': version,
												   'contexts': contexts,
												   'user': request.user,
												   'has_permission': mysite.has_permission(request),
												   'site_url': mysite.site_url,
												   'title': 'Review a Version'
												   })
