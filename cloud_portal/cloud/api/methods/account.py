from rest_framework import status
from rest_framework.decorators import api_view
from rest_framework.response import Response



@api_view(['GET', 'POST'])
def index(request):
    """
    List all snippets, or create a new snippet.
    """
    if request.method == 'GET':
        # get authorized user here
        user = 0
        serializer = UserSerializer(user, many=False)
        return Response(serializer.data)

    elif request.method == 'POST':
        serializer = UserSerializer(data=request.data)
        if serializer.is_valid():
            serializer.save()
            return Response(serializer.data, status=status.HTTP_201_CREATED)
        return Response(serializer.errors, status=status.HTTP_400_BAD_REQUEST)

def login(request):
    #authorize user here
    #return user

    user = 0
    serializer = UserSerializer(user, many=False)
    return Response(serializer.data)

def logout(request):
    return Response(false, status=status.HTTP_400_BAD_REQUEST)

def register(request):
    return Response(false, status=status.HTTP_400_BAD_REQUEST)

def activate(request):
    return Response(false, status=status.HTTP_400_BAD_REQUEST)

def restorePassword(request):
    return Response(false, status=status.HTTP_400_BAD_REQUEST)

def changePassword(request):
    return Response(false, status=status.HTTP_400_BAD_REQUEST)
