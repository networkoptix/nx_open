__author__ = 'noptix'
from api.models import Account
from rest_framework import serializers

class CreateAccountSerializer(serializers.Serializer): # ModelSerializer
    password = serializers.CharField(required=True, allow_blank=True, max_length=255)
    email = serializers.CharField(required=True, allow_blank=False, max_length=255)
    firstName = serializers.CharField(required=False, allow_blank=True, max_length=255)
    lastName = serializers.CharField(required=False, allow_blank=True, max_length=255)
    subscribe = serializers.BooleanField(required=False)

    def validate_email(self, value):
        return True;

    def create(self, validated_data):
        password = validated_data.get('password', None)
        email = validated_data.get('email', None)
        #TODO: validate email here again
        return Account.objects.create_user(email, password, **validated_data)

class AccountSerializer(serializers.Serializer): # ModelSerializer
    class Meta:
        model = Account
        fields = ('id', 'email', 'firstName', 'lastName', 'subscribe')

    def validate_email(self, value):
        return True;