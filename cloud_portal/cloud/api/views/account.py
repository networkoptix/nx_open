from rest_framework import status
from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from account_serializers import AccountSerializer, CreateAccountSerializer, AccountUpdaterSerializer
from rest_framework.permissions import AllowAny, IsAuthenticated
import django
import logging

from api.controllers.cloud_api import account

logger = logging.getLogger('django')


@api_view(['POST'])
@permission_classes((AllowAny, ))
def register(request):
    logger.debug("register")
    logger.debug(request.data)

    serializer = CreateAccountSerializer(data=request.data)
    if serializer.is_valid():
        serializer.save()
        return Response(True, status=status.HTTP_201_CREATED)
    return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)

@api_view(['POST'])
@permission_classes(( AllowAny, ))
def login(request):
    #authorize user here
    #return user

    logger.debug("login")
    logger.debug(request.data)

    user = django.contrib.auth.authenticate(username=request.data['email'], password=request.data['password'])

    logger.debug(user)

    if user is not None:
        django.contrib.auth.login(request, user)
        request.session['password'] = request.data['password'] # TODO: This is awful security hole! But I can't remove it now, because I need password for future requests
    else:
        return Response(False, status=401)

    serializer = AccountSerializer(user, many=False)
    return Response(serializer.data)

@api_view(['POST'])
@permission_classes((IsAuthenticated, ))
def logout(request):
    django.contrib.auth.logout(request)
    return Response(True, status=200)





@api_view(['GET', 'POST'])
@permission_classes((IsAuthenticated, ))
def index(request):

    logger.debug("index")

    """
    List all snippets, or create a new snippet.
    """
    if not request.user.is_authenticated():
        return Response(False, status=401)

    if request.method == 'GET':
        # get authorized user here
        serializer = AccountSerializer(request.user, many=False)

        logger.debug(request.user.email)
        return Response(serializer.data)

    elif request.method == 'POST':

        logger.debug("update")
        logger.debug(request.data)
        logger.debug(request.user)

        serializer = AccountUpdaterSerializer(request.user, data=request.data)
        if serializer.is_valid():

            success = account.update(request.user.email, request.session['password'], request.user.first_name, request.user.last_name)
            #if not success:
            #    return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)
            serializer.save()

            logger.debug("updated")
            return Response(serializer.data, status=status.HTTP_201_CREATED)

        logger.debug("serializer failed")
        return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)



@api_view(['POST'])
@permission_classes((IsAuthenticated, ))
def changePassword(request):
    old_password = request.data['old_password']
    new_password = request.data['new_password']

    success = account.changePassword(request.user.email, old_password, new_password)
    if not success:
        return Response(False, status=status.HTTP_400_BAD_REQUEST)
    request.session['password'] = new_password
    return Response(False, status=status.HTTP_201_CREATED)



@api_view(['POST'])
@permission_classes(( AllowAny, ))
def activate(request):
    return Response(False, status=status.HTTP_400_BAD_REQUEST)

@api_view(['POST'])
@permission_classes(( AllowAny, ))
def restorePassword(request):
    return Response(False, status=status.HTTP_400_BAD_REQUEST)
