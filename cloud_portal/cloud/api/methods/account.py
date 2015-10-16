from rest_framework import status
from rest_framework.decorators import api_view
from rest_framework.response import Response
from accound_models import AccountSerializer, CreateAccountSerializer
import django


@api_view(['GET', 'POST'])
def index(request):
    """
    List all snippets, or create a new snippet.
    """
    if not request.user.is_authenticated():
        return Response(False, status=401)

    if request.method == 'GET':
        # get authorized user here
        serializer = AccountSerializer(request.user, many=False)
        return Response(serializer.data)

    elif request.method == 'POST':
        serializer = AccountSerializer(request.user, data=request.data)
        if serializer.is_valid():
            serializer.save()
            return Response(serializer.data, status=status.HTTP_201_CREATED)
        return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)

@api_view(['POST'])
def register(request):
    serializer = CreateAccountSerializer(data=request.data)
    if serializer.is_valid():
        serializer.save()
        return Response(True, status=status.HTTP_201_CREATED)
    return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)

@api_view(['POST'])
def login(request):
    #authorize user here
    #return user

    user = django.contrib.auth.authenticate(username=request.data.email, password=request.data.password)
    if user is not None:
        if user.is_active:
            django.contrib.auth.login(request, user)
        else:
            return Response(False, status=401)
    else:
        return Response(False, status=401)

    serializer = AccountSerializer(user, many=False)
    return Response(serializer.data)

@api_view(['POST'])
def logout(request):
    if not request.user.is_authenticated():
        return Response(False, status=401)
    django.contrib.auth.logout(request)
    return Response(False, status=status.HTTP_400_BAD_REQUEST)






@api_view(['POST'])
def activate(request):
    return Response(False, status=status.HTTP_400_BAD_REQUEST)

@api_view(['POST'])
def restorePassword(request):
    return Response(False, status=status.HTTP_400_BAD_REQUEST)

@api_view(['POST'])
def changePassword(request):
    return Response(False, status=status.HTTP_400_BAD_REQUEST)
