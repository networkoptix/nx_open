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


def get_context_and_language(request, context_id, language_code, customization):
    context = Context.objects.get(id=context_id) if context_id else None
    language = Language.objects.get(code=language_code) if language_code else None

    if request.method == "POST":
        if not context and 'context' in request.POST and request.POST['context']:
            context = Context.objects.get(id=request.POST['context'])

        if not language and 'language' in request.POST and request.POST['language']:
            language = Language.objects.get(code=request.POST['language'])

    if not language:
        if 'language' in request.session:
            language = Language.objects.get(code=request.session['language'])
        else:
            language = customization.default_language

    return context, language


def initialize_context_editor(request, context_id, language_code):
    customization = Customization.objects.get(name=settings.CUSTOMIZATION)
    context, language = get_context_and_language(request, context_id, language_code, customization)

    form_initialization = {'language': language.code}

    if context:
        form_initialization['context'] = context.id

    form = CustomContextForm(
        initial=form_initialization)

    if context:
        form.add_fields(context, customization, language, request.user)

    return context, customization, language, form


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


def context_editor_action(request, context_id, language_code):
    context, customization, language, form = initialize_context_editor(request, context_id, language_code)
    request_data = request.POST
    request_files = request.FILES

    saved = True
    upload_errors = []
    preview_link = ""
    saved_msg = "Changes have been saved."

    if not (request.user.is_superuser or request.user.has_perm('cms.edit_advanced'))\
            and advanced_touched_without_permission(request_data, customization, context.datastructure_set.all()):
        raise PermissionDenied

    if 'languageChanged' in request_data and 'currentLanguage' in request_data and request_data['currentLanguage']:
        last_language = Language.objects.get(
            code=request_data['currentLanguage'])

        upload_errors = save_unrevisioned_records(context, customization, last_language,
                                                  context.datastructure_set.all(), request_data,
                                                  request_files, request.user)

    elif 'Preview' in request_data or 'SaveDraft' in request_data:
        upload_errors = save_unrevisioned_records(context, customization, language, context.datastructure_set.all(),
                                                  request_data, request_files, request.user)

        if 'Preview' in request_data:
            saved_msg += " Preview has been created."

    elif 'SendReview' in request_data:
        upload_errors = send_version_for_review(context, customization, language, context.datastructure_set.all(),
                                                context.product, request_data, request_files, request.user)
        saved_msg += " A new version has been created."

    else:
        saved = False

    if upload_errors:
        add_upload_error_messages(request, upload_errors)

    if saved:
        messages.success(request, saved_msg)
        preview_link = generate_preview_link(context)

    return context, customization, language, form, preview_link


# Create your views here.
@require_http_methods(["GET", "POST"])
@permission_required('cms.edit_content')
def page_editor(request, context_id=None, language_code=None):
    if request.method == "GET":
        context, customization, language, form = initialize_context_editor(request, context_id, language_code)
        preview_link = ""

    else:
        if not request.user.has_perm('cms.edit_content'):
            raise PermissionDenied

        context, customization, language, form, preview_link = context_editor_action(request, context_id, language_code)

        if 'SendReview' in request.POST:
            return redirect(reverse('version', args=[ContentVersion.objects.latest('created_date').id]))

    return render(request, 'context_editor.html',
                  {'context': context,
                   'form': form,
                   'customization': customization,
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
def version_action(request, version_id=None):
    preview_flag = ""
    if not ContentVersion.objects.filter(id=version_id).exists():
        defaults.bad_request("Version does not exist")

    if "Preview" in request.POST:
        generate_preview(send_to_review=True)
        preview_flag = "?preview"

    elif "Publish" in request.POST:
        if not request.user.has_perm('cms.publish_version'):
            raise PermissionDenied
        customization = Customization.objects.get(name=settings.CUSTOMIZATION)
        publishing_errors = publish_latest_version(customization, request.user)
        if publishing_errors:
            messages.error(request, "Version {} {}".format(version_id, publishing_errors))
        else:
            messages.success(request, "Version {} has been published".format(version_id))

    elif "Force Update" in request.POST:
        if not request.user.has_perm('cms.force_update'):
            raise PermissionDenied
        filldata.init_skin(settings.CUSTOMIZATION)
        messages.success(request, "Version {} was force updated ".format(version_id))

    else:
        return defaults.bad_request("File does not exist")

    return redirect(reverse('version', args=[version_id]) + preview_flag)


@require_http_methods(["GET"])
@permission_required('cms.change_contentversion')
def version(request, version_id=None):
    preview_link = ""
    if 'preview' in request.GET:
        preview_link = generate_preview_link()
    version = ContentVersion.objects.get(id=version_id)
    contexts = get_records_for_version(version)
    return render(request, 'review_records.html', {'version': version,
                                                   'contexts': contexts,
                                                   'preview_link': preview_link,
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
