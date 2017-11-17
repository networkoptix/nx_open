from rest_framework.decorators import api_view, permission_classes
from rest_framework.permissions import AllowAny, IsAuthenticated
from api.controllers import cloud_api, cloud_gateway

from api.helpers.exceptions import handle_exceptions, api_success, require_params

from cloud import settings
import hashlib
import base64


@api_view(['GET'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def system(request, system_id):
    data = cloud_api.System.get(request.session['login'], request.session['password'], system_id)
    return api_success(data['systems'])


@api_view(['GET'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def list_systems(request):
    data = cloud_api.System.list(request.session['login'], request.session['password'])
    return api_success(data['systems'])


@api_view(['GET', 'POST'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def sharing(request, system_id):
    if request.method == 'GET':
        # get authorized user here
        data = cloud_api.System.users(request.session['login'], request.session['password'], system_id)
        return api_success(data['sharing'])

    elif request.method == 'POST':
        require_params(request, ('user_email', 'role'))
        # 2. share or change sharing
        user_email = request.data['user_email'].lower()
        data = cloud_api.System.share(request.session['login'], request.session['password'], system_id,
                                      user_email,
                                      request.data['role'])

        return api_success(data)


def md5(data):
    m = hashlib.md5()
    m.update(data)
    return m.hexdigest()


def digest(login, password, realm, nonce, method):
    dig = md5(login + ':' + realm + ':' + password)
    method = md5(method + ':')
    auth_digest = md5(dig + ':' + nonce + ':' + method)
    auth = base64.b64encode(login + ':' + nonce + ':' + auth_digest)
    return auth


@api_view(['GET'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def get_auth(request, system_id):
    data = cloud_api.System.get_nonce(request.session['login'], request.session['password'], system_id)
    nonce = data["nonce"]
    realm = settings.CLOUD_CONNECT['password_realm']
    auth_get = digest(request.session['login'], request.session['password'], realm, nonce, 'GET')
    auth_post = digest(request.session['login'], request.session['password'], realm, nonce, 'POST')
    auth_play = digest(request.session['login'], request.session['password'], realm, nonce, 'PLAY')
    return api_success({'authGet': auth_get, 'authPost': auth_post, 'authPlay': auth_play})


@api_view(['POST'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def rename(request, system_id):
    require_params(request, ('name',))
    data = cloud_api.System.rename(request.session['login'], request.session['password'], system_id,
                                   request.data['name'])
    return api_success(data)


@api_view(['POST'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def merge(request):
    require_params(request, ('master_system_id', 'slave_system_id',))
    data = cloud_api.System.merge(request.session['login'], request.session['password'],
                                  request.data['master_system_id'], request.data['slave_system_id'])
    return api_success(data)


@api_view(['GET'])
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def access_roles(request, system_id):
    data = cloud_api.System.access_roles(request.session['login'], request.session['password'], system_id)
    return api_success(data['accessRoles'])


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def disconnect(request):
    require_params(request, ('system_id', 'password'))

    if request.user.is_authenticated():
        cloud_api.System.unbind(request.user.email, request.data['password'], request.data['system_id'])
        return api_success()

    require_params(request, ('email', 'password'))
    email = request.data['email'].lower()
    cloud_api.System.unbind(email, request.data['password'], request.data['system_id'])
    return api_success()


@api_view(['POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def connect(request):
    require_params(request, ('name',))
    if request.user.is_authenticated():
        data = cloud_api.System.bind(request.session['login'], request.session['password'], request.data['system_id'])
        return api_success(data)

    require_params(request, ('email', 'password'))
    email = request.data['email'].lower()
    data = cloud_api.System.bind(email, request.data['password'], request.data['name'])
    return api_success(data)


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def proxy(request, system_id, system_url):
    email = None
    password = None

    full_url = request.get_full_path()
    position = full_url.find('?')
    if position > -1:
        system_url += full_url[position:]

    if request.user.is_authenticated():
        email = request.session['login']
        password = request.session['password']

    if request.method == 'GET':
        data = cloud_gateway.get(system_id, system_url, email=email, password=password)
        return api_success(data)
    elif request.method == 'POST':
        data = cloud_gateway.post(system_id, system_url, request.data, email=email, password=password)
        return api_success(data)

    return None
