import json
from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated, AllowAny

from cloud import settings
from api.helpers.exceptions import api_success, handle_exceptions

from cms.controllers.filldata import process_context_structure
from cms.models import Context, DataStructure, Product, ProductCustomizationReview, ProductType,\
    UserGroupsToProductPermissions, get_cloud_portal_product

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
            current_version = integration.version_id()
            if show_pending:
                current_version = ProductCustomizationReview.objects.filter(version__product=integration,
                                                                            state=PENDING).latest('id').version.id
                integration_dict['pending'] = True

            if current_version == 0:
                continue

            for context in contexts:
                # Make context json friendly
                context_name = context.name
                context_name = context_name[0].lower() + context_name[1:]
                context_name = context_name.replace(' ', '')

                context_dict = {}
                for datastructure in context.datastructure_set.all():
                    ds_name = datastructure.name
                    if not datastructure.public:
                        continue

                    record_value = datastructure.find_actual_value(product=integration,
                                                                   version_id=current_version,
                                                                   draft=show_pending)

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
                    if context.name == "Download Files":
                        downloads_order = {}
                        for datastructure in context.datastructure_set.all():
                            downloads_order[datastructure.name] = datastructure.order
                        integration_dict[context_name+'Order'] = downloads_order

            if not integration_dict:
                continue

            for global_context in global_contexts:
                process_context_structure(cloud_portal, global_context, integration_dict, None, current_version, False, False)

            integration_dict['id'] = integration.id
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
@permission_classes((AllowAny, ))
def get_integrations(request):
    integrations = Product.objects.filter(product_type__type=INTEGRATION,
                                          customizations__name__in=[settings.CUSTOMIZATION])

    if not integrations.exists():
        return api_success([])
    integration_list = []
    drafts = Product.objects.\
        filter(product_type__type=INTEGRATION,
               contentversion__productcustomizationreview__state=PENDING,
               contentversion__productcustomizationreview__customization__name=settings.CUSTOMIZATION)

    # Users with manager permissions all accepted products and pending drafts
    if UserGroupsToProductPermissions.\
            check_customization_permission(request.user, settings.CUSTOMIZATION, 'cms.publish_version'):
        integration_list = make_integrations_json(drafts, show_pending=True)
    elif drafts.filter(created_by=request.user).exists():
        integration_list = make_integrations_json(drafts.filter(created_by=request.user), show_pending=True)

    integration_list.extend(make_integrations_json(integrations))
    return api_success({'data': integration_list})
