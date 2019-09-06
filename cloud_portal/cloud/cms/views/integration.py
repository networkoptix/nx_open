import json
from rest_framework import status
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated, AllowAny

from cloud import settings
from api.helpers.exceptions import api_success, handle_exceptions

from cms.controllers.filldata import global_contexts_to_dict, process_context_structure
from cms.models import Context, DataStructure, Product, ProductCustomizationReview, ProductType,\
    UserGroupsToProductPermissions, cloud_portal_customization_cache

CLOUD_PORTAL = ProductType.PRODUCT_TYPES.cloud_portal
INTEGRATION = ProductType.PRODUCT_TYPES.integration
ACCEPTED = ProductCustomizationReview.REVIEW_STATES.accepted
PENDING = ProductCustomizationReview.REVIEW_STATES.pending


def make_integrations_json(integrations, contexts=None, show_pending=False, show_drafts=False):
    integrations_json = []

    if not contexts:
        contexts = Context.objects.filter(product_type__type=INTEGRATION)

    global_contexts = Context.objects.filter(product_type__type=CLOUD_PORTAL, is_global=True)
    cloud_portal = Product.objects.filter(product_type__type=CLOUD_PORTAL,
                                          customizations__name__in=[settings.CUSTOMIZATION]).first()

    if cloud_portal:
        global_contexts_dict = global_contexts_to_dict(global_contexts, cloud_portal)

        for integration in integrations:
            integration_dict = {}
            current_version = integration.version_id()

            if show_pending:
                pending_version = ProductCustomizationReview.objects.filter(version__id__gt=current_version,
                                                                            version__product=integration,
                                                                            customization__name=settings.CUSTOMIZATION,
                                                                            state=PENDING).last()

                if not pending_version:
                    continue
                current_version = pending_version.version.id

            if show_drafts:
                if integration.preview_status != Product.PREVIEW_STATUS.draft:
                    continue
                current_version = None

            if show_drafts or show_pending:
                integration_dict['pending'] = show_pending
                integration_dict['draft'] = show_drafts

            if current_version == 0:
                continue

            for context in contexts:
                # Make context json friendly
                context_name = context.name

                context_dict = {}
                for datastructure in context.datastructure_set.all():
                    ds_name = datastructure.name
                    if not datastructure.public:
                        continue

                    record_value = datastructure.find_actual_value(product=integration,
                                                                   version_id=current_version,
                                                                   draft=show_pending or show_drafts)

                    if not record_value:
                        continue

                    context_dict[ds_name] = record_value

                    # If the DataStructure type is select and the multi flag is true we need to make the value an array
                    if datastructure.type == DataStructure.DATA_TYPES.select and\
                            'multi' in datastructure.meta_settings and\
                            datastructure.meta_settings['multi']:
                        # Starts as a stringified list then turned into a list of strings
                        # "['1', '2', '3']" -> [u'1', u'2', u'3'] -> ['1', '2', '3']
                        context_dict[ds_name] = map(str, json.loads(context_dict[ds_name]))

                if context_dict:
                    integration_dict[context_name] = context_dict
                    if context.name == "downloadFiles":
                        downloads_order = {}
                        for datastructure in context.datastructure_set.all():
                            downloads_order[datastructure.name] = datastructure.order
                        integration_dict[f"{context_name}Order"] = downloads_order

            if not integration_dict:
                continue

            for global_context in global_contexts:
                process_context_structure(cloud_portal, global_context, integration_dict, None,
                                          current_version, False, False, context_dict=global_contexts_dict)

            integration_dict['id'] = integration.id
            integrations_json.append(integration_dict)

    return integrations_json


def check_integration_store_enabled():
    return cloud_portal_customization_cache(settings.CUSTOMIZATION, 'config')['integration_store_enabled']


@api_view(("GET", ))
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def get_integration(request, product_id=None):
    draft = "draft" in request.GET
    review = "pending" in request.GET
    if not product_id:
        return api_success("Integration not found.", status_code=status.HTTP_404_NOT_FOUND)

    product_id = int(product_id)
    integration = Product.objects.filter(product_type__type=INTEGRATION,
                                         customizations__name__in=[settings.CUSTOMIZATION],
                                         id=product_id).last()

    if not integration:
        return api_success("Integration not found.", status_code=status.HTTP_404_NOT_FOUND)

    if draft:
        if integration.id not in request.user.products:
            return api_success("You do not have permission to view this draft", status_code=status.HTTP_403_FORBIDDEN)

        if integration.preview_status != Product.PREVIEW_STATUS.draft:
            return api_success("Draft does not exist for this integration", status_code=status.HTTP_404_NOT_FOUND)

    return api_success(make_integrations_json([integration], show_pending=review, show_drafts=draft))


@api_view(("GET", ))
@permission_classes((AllowAny, ))
def get_integrations(request):
    is_enabled = check_integration_store_enabled()
    integrations = Product.objects.filter(product_type__type=INTEGRATION,
                                          customizations__name__in=[settings.CUSTOMIZATION])

    if not integrations.exists():
        return api_success([])
    integration_list = []

    # Only known users can see Drafts and reviews
    if not request.user.is_anonymous:
        drafts = Product.objects. \
            filter(product_type__type=INTEGRATION,
                   contentversion__productcustomizationreview__customization__name=settings.CUSTOMIZATION).distinct()

        # Users without manager permissions will see only their integration (accepted, reviews, drafts).
        if not UserGroupsToProductPermissions.\
                check_customization_permission(request.user, settings.CUSTOMIZATION, 'cms.publish_version'):
            drafts = drafts.filter(id__in=request.user.products).distinct()
            integration_list = make_integrations_json(drafts.filter(preview_status=Product.PREVIEW_STATUS.draft),
                                                      show_drafts=True)
            # If the integration store is disabled show developers their approved integrations
            if not is_enabled:
                integration_list.extend(make_integrations_json(drafts))

        # If the integration store is disabled Manager level users will see all accepted and pending integrations
        elif not is_enabled:
            integration_list.extend(make_integrations_json(integrations))

        # Shows pending reviews. If the users is not a manager they will only see their pending reviews
        # Otherwise they will see all of the pending reviews
        drafts = drafts.filter(contentversion__productcustomizationreview__state=PENDING)
        integration_list.extend(make_integrations_json(drafts, show_pending=True))

    if is_enabled:
        integration_list.extend(make_integrations_json(integrations))
    return api_success({'data': integration_list})
