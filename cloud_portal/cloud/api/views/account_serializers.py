from api.models import Account
from rest_framework import serializers
import django


class CreateAccountSerializer(serializers.Serializer):  # ModelSerializer
    password = serializers.CharField(required=True, allow_blank=False, max_length=255)
    email = serializers.CharField(required=True, allow_blank=False, max_length=255)
    first_name = serializers.CharField(required=False, allow_blank=True, max_length=255)
    last_name = serializers.CharField(required=False, allow_blank=True, max_length=255)
    subscribe = serializers.BooleanField(required=False)

    @staticmethod
    def validate_email(value):
        django.core.validators.validate_email(value)
        return value

    @staticmethod
    def create(validated_data):
        return Account.objects.create_user(**validated_data)


class AccountSerializer(serializers.ModelSerializer):  # ModelSerializer
    class Meta:
        model = Account
        fields = ('email', 'first_name', 'last_name', 'subscribe')


class AccountUpdateSerializer(serializers.ModelSerializer):  # ModelSerializer
    class Meta:
        model = Account
        fields = ('first_name', 'last_name', 'subscribe')
