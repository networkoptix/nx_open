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
import json
from cloud import settings
from cms.controllers import filldata, generate_structure, modify_db, structure
from cms.forms import *

from django.contrib.admin import AdminSite


class MyAdminSite(AdminSite):
    pass
mysite = MyAdminSite()


# Used to get the context and language models
def get_context_and_language(request, context_id, language_code, customization):
    context = Context.objects.get(id=context_id) if context_id else None
    language = Language.by_code(language_code)

    # Using info in the post request we set the context and language if they are not set already
    if request.method == "POST":
        if not context and 'context' in request.POST and request.POST['context']:
            context = Context.objects.get(id=request.POST['context'])

        if not language and 'language' in request.POST and request.POST['language']:
            language = Language.by_code(request.POST['language'])

    # If we are using a GET request and no language is set then we much set it to the users session or the default one
    if not language:
        if 'admin_language' in request.session:
            language = Language.by_code(request.session['admin_language'])
        else:
            language = customization.default_language

    request.session['admin_language'] = language.code
    return context, language


# This builds the custom 'edit page' form
def initialize_form(product, context, customization, language, user):
    form_initialization = {'language': language.code}

    if context:
        form_initialization['context'] = context.id

    form = CustomContextForm(
        initial=form_initialization)

    if context:
        form.add_fields(product, context, customization, language, user)

    return form


# This is used to get the context, customization and language from request information
def get_info_for_context_editor(request, context_id, language_code):
    customization = Customization.objects.get(name=settings.CUSTOMIZATION)
    context, language = get_context_and_language(request, context_id, language_code, customization)

    return context, customization, language


# If there are any errors they will be added to the django messages that show up in the response
def add_upload_error_messages(request, errors):
    for error in errors:
        messages.error(
            request, "Upload error for {}. {}".format(error[0], error[1]))


# Used to make sure users without advanced permission don't modify advanced DataStructures
def advanced_touched_without_permission(request_data, customization, data_structures):
    for ds_name in request_data:
        data_structure = data_structures.filter(name=ds_name)
        if data_structure.exists() and data_structure[0].advanced:
            data_record = data_structure[0].datarecord_set.filter(customization=customization)

            if data_record.exists():
                db_record_value = data_record.latest('created_date').value
            else:
                db_record_value = data_structure[0].default

            if request_data[ds_name] != db_record_value:
                return True

    return False


# Handles when users save, create previews, or create reviews
def context_editor_action(request, product, context_id, language_code):
    context, customization, language = get_info_for_context_editor(request, context_id, language_code)

    request_data = request.POST
    request_files = request.FILES

    preview_link = ""
    saved_msg = "Changes have been saved."

    if not (request.user.is_superuser or request.user.has_perm('cms.edit_advanced'))\
            and advanced_touched_without_permission(request_data, customization, context.datastructure_set.all()):
        raise PermissionDenied

    if any(action in request_data for action in ['languageChanged', 'Preview', 'SaveDraft', 'SendReview']):
        current_lang = language
        if 'languageChanged' in request_data and 'currentLanguage' in request_data and request_data['currentLanguage']:
            current_lang = Language.by_code(request_data['currentLanguage'])

        upload_errors = modify_db.save_unrevisioned_records(product, context, customization,
                                                            current_lang, context.datastructure_set.all(),
                                                            request_data, request_files, request.user)

        if 'Preview' in request_data:
            saved_msg += " Preview has been created."

        if 'SendReview' in request_data:
            if upload_errors:
                warning_no_error_msg = "Cannot have any errors when sending for review."
                messages.warning(request, "{} - {}".format(product.name, warning_no_error_msg))
            else:
                modify_db.send_version_for_review(product, customization, request.user)
                saved_msg += " A new version has been created."

        if upload_errors:
            add_upload_error_messages(request, upload_errors)
        else:
            messages.success(request, saved_msg)
            preview_link = modify_db.generate_preview_link(context)

    # The form is made here so that all of the changes to fields are sent with the new form
    form = initialize_form(product, context, customization, language, request.user)

    return context, customization, language, form, preview_link


# Create your views here.
@require_http_methods(["GET", "POST"])
@permission_required('cms.edit_content')
def page_editor(request, context_id=None, language_code=None):
    product = get_cloud_portal_product()
    if request.method == "GET":
        # Broken into two functions so that they can be reused in the context_editor_action function
        context, customization, language = get_info_for_context_editor(request, context_id, language_code)
        form = initialize_form(product, context, customization, language, request.user)
        preview_link = ""

    else:
        if not request.user.has_perm('cms.edit_content'):
            raise PermissionDenied

        context, customization, language, form, preview_link = context_editor_action(request, product, context_id, language_code)

        if 'SendReview' in request.POST and preview_link:
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
                   'title': 'Edit %s for %s' % (context.name, product.name)})


@require_http_methods(["POST"])
@permission_required('cms.change_contentversion')
def version_action(request, version_id=None):
    preview_flag = ""
    if not ContentVersion.objects.filter(id=version_id).exists():
        defaults.bad_request("Version does not exist")

    customization = Customization.objects.get(name=settings.CUSTOMIZATION)
    product = get_cloud_portal_product()

    if "Preview" in request.POST:
        modify_db.generate_preview(product, customization, version_id=version_id, send_to_review=True)
        preview_flag = "?preview"

    elif "Publish" in request.POST:
        if not request.user.has_perm('cms.publish_version'):
            raise PermissionDenied
        publishing_errors = modify_db.publish_latest_version(product, customization, version_id, request.user)
        if publishing_errors:
            messages.error(request, "Version {} {}".format(version_id, publishing_errors))
        else:
            messages.success(request, "Version {} has been published".format(version_id))

    elif "Force Update" in request.POST:
        if not request.user.has_perm('cms.force_update'):
            raise PermissionDenied

        filldata.init_skin(product, customization.name)
        messages.success(request, "Version {} was force updated ".format(version_id))

    else:
        return defaults.bad_request("File does not exist")

    return redirect(reverse('version', args=[version_id]) + preview_flag)


@require_http_methods(["GET"])
@permission_required('cms.change_contentversion')
def version(request, version_id=None):
    preview_link = ""
    version = ContentVersion.objects.get(id=version_id)
    contexts = modify_db.get_records_for_version(version)
    #else happens when the user makes a revision without any changes
    if contexts.values():
        product = get_cloud_portal_product()
        context = None
        if 'preview' in request.GET:
            if len(contexts) == 1:
                context = Context.objects.get(name=contexts.keys()[0])
            preview_link = modify_db.generate_preview_link(context)
    else:
        product = {'can_preview': False, 'name': ""}
    return render(request, 'review_records.html', {'version': version,
                                                   'contexts': contexts,
                                                   'product': product,
                                                   'preview_link': preview_link,
                                                   'user': request.user,
                                                   'has_permission': mysite.has_permission(request),
                                                   'site_url': mysite.site_url,
                                                   'site_header': admin.site.site_header,
                                                   'site_title': admin.site.site_title
                                                   })


def response_attachment(data, filename, content_type):
    response = HttpResponse(data, content_type=content_type)
    response['Content-Disposition'] = 'attachment; filename=%s' % filename
    response.set_cookie('filename', filename, max_age=10);
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
        update_content = action == 'update_content'

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
                data, log_messages = generate_structure.from_zip(file, product)
                content = json.dumps(data, ensure_ascii=False, indent=4, separators=(',', ': '))
                for error in log_messages:
                    messages.error(request, "Error with {} problem with {}".format(error['file'], error['extension']))
                return response_attachment(content, 'structure.json', 'application/json')
            log_messages = structure.process_zip(file, request.user, product, update_structure, update_content)
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
    product = get_cloud_portal_product()
    customization = Customization.objects.get(name=settings.CUSTOMIZATION)

    language_code = request.GET['lang'] if 'lang' in request.GET else None
    version_id = request.GET['version_id'] if 'version_id' in request.GET else None
    preview = 'draft' in request.GET
    file = filldata.read_customized_file(path, product, customization, language_code, version_id, preview)
    if file:
        return response_attachment(file, os.path.basename(path), "application")
    raise defaults.page_not_found("File does not exist")


@require_http_methods(["GET"])
def download_package(request, product_id, customization_name=None):
    product = Product.objects.get(id=product_id)
    if not customization_name:
        customization = product.customizations.first()
    else:
        customization = Customization.objects.get(name=customization_name)

    version_id = request.GET['version_id'] if 'version_id' in request.GET else None
    preview = 'draft' in request.GET
    zipped_data = filldata.get_zip_package(product, customization, preview, version_id)
    return response_attachment(zipped_data, product.name + ".zip", "application/zip")
