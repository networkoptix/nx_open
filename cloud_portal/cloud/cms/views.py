from __future__ import absolute_import
from django.views.decorators.http import require_http_methods
from django.views import defaults
from django.contrib import messages
from django.contrib.auth.decorators import permission_required
from django.core.exceptions import PermissionDenied
from django.shortcuts import render, redirect
from django.urls import reverse
from django.contrib import admin
from django.http.response import HttpResponse, HttpResponseBadRequest, Http404

import os
from cloud import settings
import json
from .controllers import structure, generate_structure

from .controllers.modify_db import *
from .forms import *
from .controllers import filldata

from django.contrib.admin import AdminSite


class MyAdminSite(AdminSite):
    pass
mysite = MyAdminSite()


def get_post_parameters(request, context_id, language_id):
    customization = Customization.objects.get(name=settings.CUSTOMIZATION)
    context, language = get_context_and_language(
        request.POST, context_id, language_id)
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
        form.add_fields(context, customization, language, request.user)

    return context, form, language


def add_upload_error_messages(request, errors):
    for error in errors:
        messages.error(
            request, "Upload error for {}. {}".format(error[0], error[1]))


def advanced_touched_without_permission(request_data, customization, data_structures):
    for ds_name in request_data:
        data_structure = data_structures.filter(name=ds_name)
        if data_structure.exists() and data_structure[0].advanced:
            data_record = data_structure[0].datarecord_set.filter(customization=customization)

            if data_record.exists():
                db_record_value = data_record.latest('created_date').value
            else:
                db_record_value = data_structure.default

            if request_data[ds_name] != db_record_value:
                    return True

    return False


def handle_post_context_edit_view(request, context_id, language_id):
    context, language, form, customization, user = get_post_parameters(
        request, context_id, language_id)
    request_data = request.POST
    request_files = request.FILES

    saved = True
    upload_errors = []
    if not (user.is_superuser or user.has_perm('cms.edit_advanced'))\
            and advanced_touched_without_permission(request_data, customization, context.datastructure_set.all()):
        raise PermissionDenied
    if 'languageChanged' in request_data:
        if 'currentLanguage' in request_data \
           and request_data['currentLanguage']:
            last_language = Language.objects.get(
                id=request_data['currentLanguage'])
            upload_errors = save_unrevisioned_records(context,
                customization, last_language, context.datastructure_set.all(),
                request_data, request_files, user)
        else:
            saved = False

        if upload_errors:
            add_upload_error_messages(request, upload_errors)

        messages.success(request, "Changes have been saved.")

    elif 'Preview' in request_data:
        upload_errors = save_unrevisioned_records(context,
            customization, language, context.datastructure_set.all(),
            request_data, request_files, user)

        if upload_errors:
            add_upload_error_messages(request, upload_errors)

        messages.success(request,
                         "Changes have been saved. Preview has been created.")

    elif 'SaveDraft' in request_data:
        upload_errors = save_unrevisioned_records(context,
            customization, language, context.datastructure_set.all(),
            request_data, request_files, user)

        if upload_errors:
            add_upload_error_messages(request, upload_errors)

        messages.success(request, "Changes have been saved.")

    elif 'SendReview' in request_data:
        upload_errors = send_version_for_review(context,
            customization, language, context.datastructure_set.all(),
            context.product, request_data, request_files, user)

        if upload_errors:
            add_upload_error_messages(request, upload_errors)

        messages.success(
            request,
            "Changes have been saved. A new version has been created.")

        return None, None, None, None

    preview_link = request.get_host() + generate_preview_link(context) if saved else ""

    form.add_fields(context, customization, language, user)

    return context, form, language, preview_link


# Create your views here.
@require_http_methods(["GET", "POST"])
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

        if 'SendReview' in request.POST:
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


@require_http_methods(["POST"])
@permission_required('cms.change_contentversion')
def review_version_request(request, context=None, language=None):
    if "Preview" in request.POST:
        preview_link = "//" + request.get_host() + generate_preview(send_to_review=True)
        return redirect(preview_link)
    elif "Publish" in request.POST:
        if not request.user.has_perm('cms.publish_version'):
            raise PermissionDenied
        customization = Customization.objects.get(name=settings.CUSTOMIZATION)
        publishing_errors = publish_latest_version(customization, request.user)
        version = ContentVersion.objects.latest('created_date')
        if publishing_errors:
            messages.error(request, "Version " +
                             str(version.id) + publishing_errors)
        else:
            messages.success(request, "Version " +
                             str(version.id) + " has been published")
        return redirect(reverse('review_version', args=[version.id]))
    return defaults.bad_request("File does not exist")


@require_http_methods(["GET"])
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


def response_attachment(data, filename, content_type):
    response = HttpResponse(data, content_type=content_type)
    response['Content-Disposition'] = 'attachment; filename=%s' % filename
    return response


@require_http_methods(["GET", "POST"])
@permission_required('cms.change_product')
def product_settings(request, product_id):
    product = Product.objects.get(pk=product_id)
    form = None
    if request.method == "POST":
        form = ProductSettingsForm(request.POST, request.FILES)
        if not form.is_valid():
            form = None

    if form:
        action = form.cleaned_data['action']
        generate_json = action == 'generate_json'
        update_structure = action == 'update_structure'
        update_content = action == 'generate_json'

        file = request.FILES["file"]

        if file.name.endswith('json'):
            if not update_structure:
                return HttpResponseBadRequest('json is acceptable only for Updating structure')
            cms_structure = json.load(file)
            structure.update_from_object(cms_structure)
            messages.success(request, "Structure updated")
        else:
            if not file.name.endswith('zip'):
                return HttpResponseBadRequest('zip archive is expected')
            if generate_json:
                data = generate_structure.from_zip(file, product.name)
                content = json.dumps(data, ensure_ascii=False, indent=4, separators=(',', ': '))
                return response_attachment(content, 'structure.json', 'application/json')
            log_messages = structure.process_zip(file, request.user, update_structure, update_content)
            for item in log_messages:
                log_type = {
                    'info': messages.INFO,
                    'error': messages.ERROR,
                    'debug': messages.DEBUG,
                    'success': messages.SUCCESS,
                    'warning': messages.WARNING,
                }[item[0]]
            messages.add_message(request, log_type, item[1])
    else:
        form = ProductSettingsForm()

    return render(request, 'product_settings.html',
                  {'product': product,
                   'form': form,

                   'user': request.user,
                   'has_permission': mysite.has_permission(request),
                   'site_url': mysite.site_url,
                   'site_header': admin.site.site_header,
                   'site_title': admin.site.site_title,
                   'title': 'Settings for %s' % product.name})


@require_http_methods(["GET"])
def download_file(request, path):
    language_code = request.GET['lang'] if 'lang' in request.GET else None
    version_id = request.GET['version_id'] if 'version_id' in request.GET else None
    preview = 'draft' in request.GET
    file = filldata.read_customized_file(path, settings.CUSTOMIZATION, language_code, version_id, preview)
    if file:
        return response_attachment(file, os.path.basename(path), "application")
    raise defaults.page_not_found("File does not exist")


@require_http_methods(["GET"])
def download_package(request, product_name, customization_name=None):
    if not customization_name:
        customization_name = settings.CUSTOMIZATION
    version_id = request.GET['version_id'] if 'version_id' in request.GET else None
    preview = 'draft' in request.GET
    zipped_data = filldata.get_zip_package(customization_name, product_name, preview, version_id)
    return response_attachment(zipped_data, product_name+".zip", "application/zip")
