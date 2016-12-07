from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from django.core.cache import cache
from rest_framework.permissions import AllowAny
from api.helpers.exceptions import handle_exceptions, api_success, require_params, APIRequestException, ErrorCodes
import datetime
from cloud import settings


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def visited_key(request):
    if request.method == 'GET':
        # Check cache value here
        if 'key' not in request.query_params:
            raise APIRequestException('Parameter key is missing', ErrorCodes.wrong_parameters,
                                      error_data=request.query_params)
        key = 'visited_key_' + request.query_params['key']
        value = cache.get(key, False)
    elif request.method == 'POST':
        # Save cache value here
        require_params(request, ('key',))
        key = 'visited_key_' + request.data['key']
        value = datetime.datetime.now()
        print (value)
        cache.set(key, value, settings.LINKS_LIVE_TIMEOUT)
    return Response({'visited': value})

