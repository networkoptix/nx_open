from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import IsAuthenticated

from cloud import settings
from api.helpers.exceptions import APINotFoundException, api_success, handle_exceptions

from cms.controllers.filldata import process_context_structure
from cms.models import Context, DataStructure, Product, ProductCustomizationReview, ProductType

CLOUD_PORTAL = ProductType.PRODUCT_TYPES.cloud_portal
ACCEPTED = ProductCustomizationReview.REVIEW_STATES.accepted


def process_product(product):
    product_dict = {}
    version_id = ProductCustomizationReview.objects.filter(customization__name=settings.CUSTOMIZATION,
                                                           state=ACCEPTED,
                                                           version__product=product).latest('id').version.id

    for datastructure in DataStructure.objects.filter(context__product_type__product=product):
        ds_name = datastructure.label if datastructure.label else datastructure.name
        product_dict[ds_name] = datastructure.find_actual_value(product=product, version_id=version_id)

    # context = product.product_type.context_set.first()

    global_contexts = Context.objects.filter(product_type__type=CLOUD_PORTAL, is_global=True)

    for global_context in global_contexts:
        process_context_structure(product, global_context, product_dict, None, version_id, False, False)

    return product_dict


@api_view(("GET", ))
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def get_integration(request):
    products = Product.objects.exclude(product_type__type=CLOUD_PORTAL)
    if products.exists() and 'product_id' in request.GET:
        return api_success(process_product(products.get(id=request.GET['product_id'])))

    raise APINotFoundException("Could not find integration", )
