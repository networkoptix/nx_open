from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated
from django.core.exceptions import ObjectDoesNotExist

from cloud import settings
from api.helpers.exceptions import APINotFoundException, api_success, handle_exceptions

from cms.controllers.filldata import process_context_structure
from cms.models import Context, DataStructure, Product, ProductCustomizationReview, ProductType

CLOUD_PORTAL = ProductType.PRODUCT_TYPES.cloud_portal
INTEGRATION = ProductType.PRODUCT_TYPES.integration
ACCEPTED = ProductCustomizationReview.REVIEW_STATES.accepted


def make_integrations_json(integrations, contexts=[]):
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
            current_version = integration.version_id()
            if current_version == 0:
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
                                                                                version_id=current_version)

            for global_context in global_contexts:
                process_context_structure(cloud_portal, global_context, integration_dict, None, current_version, False, False)

            integration_dict['id'] = integration.id
            integrations_json.append(integration_dict)

    return {'data': integrations_json}


@api_view(("GET", ))
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def get_integration(request, product_id=None):
    products = Product.objects.filter(product_type__type=INTEGRATION,
                                      customizations__name__in=[settings.CUSTOMIZATION])
    try:
        return api_success(make_integrations_json([products.get(id=product_id)]))
    except ObjectDoesNotExist:
        pass

    raise APINotFoundException("Could not find integration", )


@api_view(("GET", ))
@permission_classes((IsAuthenticated, ))
def get_integrations(request):
    integrations = Product.objects.filter(product_type__type=INTEGRATION,
                                          customizations__name__in=[settings.CUSTOMIZATION])

    if not integrations.exists():
        return api_success([])

    return api_success(make_integrations_json(integrations))
