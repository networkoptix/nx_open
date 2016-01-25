from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny, IsAuthenticated
from api.controllers import cloud_api

from api.helpers.exceptions import handle_exceptions, api_success, require_params


@api_view(['GET'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def list_systems(request):
    data = cloud_api.System.list(request.user.email, request.session['password'])
    print(data['systems'])
    return api_success(data['systems'])


@api_view(['GET', 'POST'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def sharing(request, system_id):
    if request.method == 'GET':
        # get authorized user here
        data = cloud_api.System.users(request.user.email, request.session['password'], system_id)
        return api_success(data['sharing'])

    elif request.method == 'POST':
        require_params(request, ('user_email', 'role'))
        # 2. share or change sharing
        data = cloud_api.System.share(request.user.email, request.session['password'], system_id,
                                      request.data['user_email'],
                                      request.data['role'])

        return api_success(data)


@api_view(['GET'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def access_roles(request):
    data = cloud_api.System.access_roles(request.user.email, request.session['password'])
    return api_success(data['access_roles'])
