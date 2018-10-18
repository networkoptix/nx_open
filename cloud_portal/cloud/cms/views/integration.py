from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated

from cloud import settings
from api.helpers.exceptions import api_success, handle_exceptions

from cms.controllers.filldata import process_context_structure
from cms.models import Context, Product, ProductCustomizationReview, ProductType, UserGroupsToCustomizationPermissions

CLOUD_PORTAL = ProductType.PRODUCT_TYPES.cloud_portal
INTEGRATION = ProductType.PRODUCT_TYPES.integration
ACCEPTED = ProductCustomizationReview.REVIEW_STATES.accepted
PENDING = ProductCustomizationReview.REVIEW_STATES.pending


def make_integrations_json(integrations, contexts=[], show_pending=False):
    integrations_json = []

    if not contexts:
        contexts = Context.objects.filter(product_type__type=INTEGRATION)

    global_contexts = Context.objects.filter(product_type__type=CLOUD_PORTAL, is_global=True)
    cloud_portal = Product.objects.filter(product_type__type=CLOUD_PORTAL,
                                          customizations__name__in=[settings.CUSTOMIZATION])
    if cloud_portal.exists():
        cloud_portal = cloud_portal.first()

        for integration in integrations:
            integration_dict = {}
            current_version = integration.version_id() if not show_pending else 0
            if current_version == 0 and (not show_pending or not integration.contentversion_set.filter(
                    productcustomizationreview__state=PENDING).exists()):
                continue

            for context in contexts:
                # Make context json friendly
                context_name = context.name
                context_name = context_name[0].lower() + context_name[1:]
                context_name = context_name.replace(' ', '')

                integration_dict[context_name] = {}
                for datastructure in context.datastructure_set.all():
                    ds_name = datastructure.name
                    if not datastructure.public:
                        continue
                    integration_dict[context_name][ds_name] = datastructure.find_actual_value(product=integration,
                                                                                              version_id=current_version,
                                                                                              draft=show_pending)

            for global_context in global_contexts:
                process_context_structure(cloud_portal, global_context, integration_dict, None, current_version, False, False)

            integration_dict['id'] = integration.id
            if show_pending:
                integration_dict['pending'] = current_version == 0
            integrations_json.append(integration_dict)

    return integrations_json


@api_view(("GET", ))
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def get_integration(request, product_id=None):
    products = Product.objects.filter(product_type__type=INTEGRATION,
                                      customizations__name__in=[settings.CUSTOMIZATION])

    return api_success(make_integrations_json([products.get(id=product_id)]))


@api_view(("GET", ))
@permission_classes((IsAuthenticated, ))
def get_integrations(request):
    integrations = Product.objects.filter(product_type__type=INTEGRATION,
                                          customizations__name__in=[settings.CUSTOMIZATION])

    if not integrations.exists():
        return api_success([])
    integration_list = []
    # Users with manager permissions all accepted products and pending drafts
    if UserGroupsToCustomizationPermissions.check_permission(request.user, settings.CUSTOMIZATION, 'cms.can_view_pending'):
        drafts = Product.objects.filter(product_type__type=INTEGRATION,
                                        contentversion__productcustomizationreview__state=PENDING)
        integration_list = make_integrations_json(drafts, show_pending=True)

    integration_list.extend(make_integrations_json(integrations))
    return api_success({'data': integration_list})
