from rest_framework import status
from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from account_models import AccountSerializer, CreateAccountSerializer
from rest_framework.permissions import AllowAny, IsAuthenticated
import django
import logging

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
        serializer = AccountSerializer(request.user, data=request.data)
        if serializer.is_valid():
            serializer.save()
            return Response(serializer.data, status=status.HTTP_201_CREATED)
        return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)







@api_view(['POST'])
@permission_classes(( AllowAny, ))
def activate(request):
    return Response(False, status=status.HTTP_400_BAD_REQUEST)

@api_view(['POST'])
@permission_classes(( AllowAny, ))
def restorePassword(request):
    return Response(False, status=status.HTTP_400_BAD_REQUEST)

@api_view(['POST'])
@permission_classes((IsAuthenticated, ))
def changePassword(request):
    return Response(False, status=status.HTTP_400_BAD_REQUEST)
