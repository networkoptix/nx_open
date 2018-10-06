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
def get_context_and_language(request, context_id, language_code, default_language):
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
            language = default_language

    request.session['admin_language'] = language.code
    return context, language


# This builds the custom 'edit page' form
def initialize_form(product, context, language, user):
    form_initialization = {'language': language.code}

    if context:
        form_initialization['context'] = context.id

    form = CustomContextForm(
        initial=form_initialization)

    if context:
        form.add_fields(product, context, language, user)

    return form


# If there are any errors they will be added to the django messages that show up in the response
def add_upload_error_messages(request, errors):
    for error in errors:
        messages.error(
            request, "Upload error for {}. {}".format(error[0], error[1]))


# Used to make sure users without advanced permission don't modify advanced DataStructures
def advanced_touched_without_permission(request_data, data_structures, product):
    for ds_name in request_data:
        data_structure = data_structures.filter(name=ds_name)
        if data_structure.exists() and data_structure[0].advanced:
            data_record = data_structure[0].datarecord_set.filter(product=product)

            if data_record.exists():
                db_record_value = data_record.latest('created_date').value
            else:
                db_record_value = data_structure[0].default

            if request_data[ds_name] != db_record_value:
                return True

    return False


# Handles when users save, create previews, or create reviews
def context_editor_action(request, product, context_id, language_code):
    context, language = get_context_and_language(request, context_id, language_code, product.default_language)

    request_data = request.POST
    request_files = request.FILES

    preview_link = ""
    saved_msg = "Changes have been saved."

    if not (request.user.is_superuser or request.user.has_perm('cms.edit_advanced'))\
            and advanced_touched_without_permission(request_data, context.datastructure_set.all(), product):
        raise PermissionDenied

    if any(action in request_data for action in ['languageChanged', 'Preview', 'SaveDraft', 'SendReview']):
        current_lang = language
        if 'languageChanged' in request_data and 'currentLanguage' in request_data and request_data['currentLanguage']:
            current_lang = Language.by_code(request_data['currentLanguage'])

        upload_errors = modify_db.save_unrevisioned_records(product, context,
                                                            current_lang, context.datastructure_set.all(),
                                                            request_data, request_files, request.user)

        if 'Preview' in request_data:
            saved_msg += " Preview has been created."

        if 'SendReview' in request_data:
            if upload_errors:
                warning_no_error_msg = "Cannot have any errors when sending for review."
                messages.warning(request, "{} - {}".format(product.name, warning_no_error_msg))
            else:
                modify_db.send_version_for_review(product, request.user)
                saved_msg += " A new version has been created."

        if upload_errors:
            add_upload_error_messages(request, upload_errors)
        else:
            messages.success(request, saved_msg)
            preview_link = modify_db.generate_preview_link(context)

    # The form is made here so that all of the changes to fields are sent with the new form
    form = initialize_form(product, context, language, request.user)

    return context, language, form, preview_link


# Create your views here.
@require_http_methods(["GET", "POST"])
@permission_required('cms.edit_content')
def page_editor(request, context_id=None, language_code=None, product_id=None):
    if product_id:
        product = Product.objects.get(id=product_id)

    if request.method == "GET":
        context, language = get_context_and_language(request, context_id, language_code, product.default_language)
        form = initialize_form(product, context, language, request.user)
        preview_link = ""

    else:
        if not request.user.has_perm('cms.edit_content'):
            raise PermissionDenied

        context, language, form, preview_link = context_editor_action(request, product, context_id, language_code)

        if 'SendReview' in request.POST and preview_link:
            review = ProductCustomizationReview.objects.filter(version_id=ContentVersion.objects.latest('created_date')).first()
            redirect_url = reverse('admin:cms_productcustomizationreview_change', args=(review.id,))
            return redirect(redirect_url)

    return render(request, 'context_editor.html',
                  {'context': context,
                   'form': form,
                   'product': product,
                   'language': language,
                   'preview_link': preview_link,
                   'user': request.user,
                   'has_permission': mysite.has_permission(request),
                   'site_url': mysite.site_url,
                   'site_header': admin.site.site_header,
                   'site_title': admin.site.site_title,
                   'title': 'Edit %s for %s' % (context.name, product.name)})


@require_http_methods(["POST"])
@permission_required("cms.change_productcustomizationreview")
def review(request):
    review_id = request.POST['review_id'] if 'review_id' in request.POST else None

    if not ProductCustomizationReview.objects.filter(id=review_id).exists():
        return HttpResponseBadRequest("Version does not exist")

    product_review = ProductCustomizationReview.objects.get(id=review_id)
    product = product_review.version.product

    if 'force_update' in request.POST and UserGroupsToCustomizationPermissions.check_permission(request.user,
                                                                                                settings.CUSTOMIZATION,
                                                                                                'cms.force_update'):
        if product.product_type.can_preview:
            filldata.init_skin(product)
            messages.success(request, "Version {} was force updated ".format(product_review.version.id))
        else:
            messages.error(request, "You cannot force update this product")

    elif 'publish' in request.POST and UserGroupsToCustomizationPermissions.check_permission(request.user,
                                                                                             settings.CUSTOMIZATION,
                                                                                             'cms.publish_version'):
        if product.product_type.can_preview:
            publishing_errors = modify_db.publish_latest_version(product, review_id, request.user)
            if publishing_errors:
                messages.error(request, "Version {} {}".format(product_review.version.id, publishing_errors))
            else:
                messages.success(request, "Version {} has been published".format(product_review.version.id))
        else:
            modify_db.update_draft_state(review_id, ProductCustomizationReview.REVIEW_STATES.accepted, request.user)
            messages.success(request, "Version {} has been accepted".format(product_review.version.id))

    elif 'reject' in request.POST:
        modify_db.update_draft_state(review_id, ProductCustomizationReview.REVIEW_STATES.rejected, request.user)
        messages.success(request, "Version {} has been rejected".format(product_review.version.id))

    # In the future we will use this for doing special stuff
    elif 'ask_question' in request.POST:
        pass

    else:
        messages.error(request, "Invalid option selected")

    return


@require_http_methods(["POST"])
@permission_required('cms.change_productcustomizationreview')
def make_preview(request):
    version_id = request.POST['version_id'] if 'version_id' in request.POST else None
    context = Context.objects.get(id=request.POST['context_id'])
    product = get_product_by_revision(version_id)
    if product.product_type.can_preview:
        redirect_url = modify_db.generate_preview(product, context, version_id=version_id, send_to_review=True)
    else:
        review = ProductCustomizationReview.objects.get(version_id=version_id,
                                                        customization__name=settings.CUSTOMIZATION)
        redirect_url = reverse('admin:cms_productcustomizationreview_change', args=(review.id,))
        messages.error(request, "This product can not be previewed")

    return HttpResponse(redirect_url)


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

    language_code = request.GET['lang'] if 'lang' in request.GET else None
    version_id = request.GET['version_id'] if 'version_id' in request.GET else None
    preview = 'draft' in request.GET
    file = filldata.read_customized_file(path, product, language_code, version_id, preview)
    if file:
        return response_attachment(file, os.path.basename(path), "application")
    raise defaults.page_not_found("File does not exist")


@require_http_methods(["GET"])
def download_package(request, product_id):
    product = Product.objects.get(id=product_id)

    version_id = request.GET['version_id'] if 'version_id' in request.GET else None
    preview = 'draft' in request.GET
    zipped_data = filldata.get_zip_package(product, preview, version_id)
    return response_attachment(zipped_data, product.name + ".zip", "application/zip")
