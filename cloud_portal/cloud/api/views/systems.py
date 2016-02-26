from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny, IsAuthenticated
from api.controllers import cloud_api

from api.helpers.exceptions import handle_exceptions, api_success, require_params


@api_view(['GET'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def system(request, system_id):
    data = cloud_api.System.get(request.user.email, request.session['password'], system_id)
    return api_success(data['systems'])


@api_view(['GET'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def list_systems(request):
    data = cloud_api.System.list(request.user.email, request.session['password'])
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
def access_roles(request, system_id):
    data = cloud_api.System.access_roles(request.user.email, request.session['password'], system_id)
    return api_success(data['accessRoles'])


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def disconnect(request):
    require_params(request, ('system_id',))

    if request.user.is_authenticated():
        cloud_api.System.unbind(request.user.email, request.session['password'], request.data['system_id'])
        return api_success()

    require_params(request, ('email', 'password'))
    cloud_api.System.unbind(request.data['email'], request.data['password'], request.data['system_id'])
    return api_success()


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def connect(request):
    require_params(request, ('name',))
    if request.user.is_authenticated():
        data = cloud_api.System.bind(request.user.email, request.session['password'], request.data['system_id'])
        return api_success(data)

    require_params(request, ('email', 'password'))
    data = cloud_api.System.bind(request.data['email'], request.data['password'], request.data['name'])
    return api_success(data)
