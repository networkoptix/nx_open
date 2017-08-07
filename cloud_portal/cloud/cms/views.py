from __future__ import absolute_import
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated, AllowAny, IsAdminUser
from rest_framework.response import Response

from django.contrib import messages
from django.contrib.auth.decorators import permission_required
from django.core.exceptions import PermissionDenied
from django.shortcuts import render, redirect
from django.urls import reverse
from django.contrib import admin

from cloud import settings

from .controllers.modify_db import *
from .forms import *

from django.contrib.admin import AdminSite


class MyAdminSite(AdminSite):
    pass
mysite = MyAdminSite()


def get_post_parameters(request, context_id, language_id):
    customization = Customization.objects.get(name=settings.CUSTOMIZATION)
    context, language = get_context_and_language(
        request.data, context_id, language_id)
    user = Account.objects.get(email=request.user)

    if not language and 'language' in request.session:
        languange = Language.objects.get(code=request.session['language'])

    if not language:
        language = customization.default_language

    form = CustomContextForm(
        initial={'language': language.id, 'context': context.id})

    return context, language, form, customization, user


def handle_get_view(request, context_id, language_code):
    context, language, = None, None
    customization = Customization.objects.get(name=settings.CUSTOMIZATION)
    if context_id:
        context = Context.objects.get(id=context_id)

    if language_code:
        language = Language.objects.get(code=language_code)

    elif 'language' in request.session:
        language = Language.objects.get(code=request.session['language'])

    else:
        language = customization.default_language

    language_id = language.id

    form = CustomContextForm(initial={'language': language_id})
    if context:
        form.add_fields(context, customization, language)

    return context, form, language


def add_upload_error_messages(request, errors):
    for error in errors:
        messages.error(
            request, "Upload error for {}. {}".format(error[0], error[1]))


def handle_post_context_edit_view(request, context_id, language_id):
    context, language, form, customization, user = get_post_parameters(
        request, context_id, language_id)
    request_data = request.data
    request_files = request.FILES
    preview_link = ""
    upload_errors = []

    if 'languageChanged' in request_data:
        if 'currentLanguage' in request_data \
           and request_data['currentLanguage']:
            last_language = Language.objects.get(
                id=request_data['currentLanguage'])
            upload_errors = save_unrevisioned_records(
                customization, last_language, context.datastructure_set.all(),
                request_data, request_files, user)

        if upload_errors:
            add_upload_error_messages(request._request, upload_errors)

        messages.success(request._request, "Changes have been saved.")

    elif 'Preview' in request_data:
        upload_errors = save_unrevisioned_records(
            customization, language, context.datastructure_set.all(),
            request_data, request_files, user)
        preview_link = request.get_host() + generate_preview(context)

        if upload_errors:
            add_upload_error_messages(request._request, upload_errors)

        messages.success(request._request,
                         "Changes have been saved. Preview has been created.")

    elif 'SaveDraft' in request_data:
        upload_errors = save_unrevisioned_records(
            customization, language, context.datastructure_set.all(),
            request_data, request_files, user)

        if upload_errors:
            add_upload_error_messages(request._request, upload_errors)

        messages.success(request._request, "Changes have been saved.")

    elif 'SendReview' in request_data:
        upload_errors = send_version_for_review(
            customization, language, context.datastructure_set.all(),
            context.product, request_data, request_files, user)

        if upload_errors:
            add_upload_error_messages(request._request, upload_errors)

        messages.success(
            request._request,
            "Changes have been saved. A new version has been created.")

        return None, None, None, None

    form.add_fields(context, customization, language)

    return context, form, language, preview_link


# Create your views here.
@api_view(["GET", "POST"])
@permission_required('cms.edit_content')
def context_edit_view(request, context=None, language=None):
    if request.method == "GET":
        context, form, language = handle_get_view(request, context, language)
        return render(request, 'context_editor.html',
                      {'context': context,
                       'form': form,
                       'language': language,
                       'user': request.user,
                       'has_permission': mysite.has_permission(request),
                       'site_url': mysite.site_url,
                       'site_header': admin.site.site_header,
                       'site_title': admin.site.site_title,
                       'title': 'Edit %s for %s' % (context.name, context.product.name)})

    else:
        if not request.user.has_perm('cms.edit_content'):
            raise PermissionDenied

        context, form, language, preview_link = handle_post_context_edit_view(
            request, context, language)

        if 'SendReview' in request.data:
            return redirect(reverse('review_version', args=[ContentVersion.objects.latest('created_date').id]))

        return render(request, 'context_editor.html',
                      {'context': context,
                       'form': form,
                       'language': language,
                       'preview_link': preview_link,
                       'user': request.user,
                       'has_permission': mysite.has_permission(request),
                       'site_url': mysite.site_url,
                       'site_header': admin.site.site_header,
                       'site_title': admin.site.site_title,
                       'title': 'Edit %s for %s' % (context.name, context.product.name)})


@api_view(["POST"])
@permission_required('cms.change_contentversion')
def review_version_request(request, context=None, language=None):
    if "Preview" in request.data:
        preview_link = "//" + request.get_host() + generate_preview()
        return redirect(preview_link)
    elif "Publish" in request.data:
        if not request.user.has_perm('cms.publish_version'):
            raise PermissionDenied
        customization = Customization.objects.get(name=settings.CUSTOMIZATION)
        publishing_errors = publish_latest_version(customization, request.user)
        version = ContentVersion.objects.latest('created_date')
        if publishing_errors:
            messages.error(request._request, "Version " +
                             str(version.id) + publishing_errors)
        else:
            messages.success(request._request, "Version " +
                             str(version.id) + " has been published")
        return redirect(reverse('review_version', args=[version.id]))
    return Response("Invalid")


@api_view(["GET"])
@permission_required('cms.change_contentversion')
def review_version_view(request, version_id=None):
    version = ContentVersion.objects.get(id=version_id)
    contexts = get_records_for_version(version)
    return render(request, 'review_records.html', {'version': version,
                                                   'contexts': contexts,
                                                   'user': request.user,
                                                   'has_permission': mysite.has_permission(request),
                                                   'site_url': mysite.site_url,
                                                   'site_header': admin.site.site_header,
                                                   'site_title': admin.site.site_title,
                                                   'title': 'Review a Version'
                                                   })
