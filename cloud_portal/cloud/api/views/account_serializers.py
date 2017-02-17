from api.models import Account
from rest_framework import serializers
import django
from cloud import settings


class CreateAccountSerializer(serializers.Serializer):  # ModelSerializer
    password = serializers.CharField(required=True, allow_blank=False, max_length=255,
                                     min_length=settings.PASSWORD_REQUIREMENTS['minLength'])
    email = serializers.CharField(required=True, allow_blank=False, max_length=255)
    language = serializers.CharField(required=True, allow_blank=False, max_length=7)
    first_name = serializers.CharField(required=True, allow_blank=False, max_length=255)
    last_name = serializers.CharField(required=True, allow_blank=False, max_length=255)
    subscribe = serializers.BooleanField(required=False)

    code = serializers.CharField(required=False, max_length=255)

    @staticmethod
    def validate_password(value):

        if len(value) < settings.PASSWORD_REQUIREMENTS['minLength']:
            raise serializers.ValidationError("Too short password")

        if len(value) > 255:
            raise serializers.ValidationError("Too long password")

        # Correct characters
        pattern = settings.PASSWORD_REQUIREMENTS['requiredRegex']
        if not pattern.match(value):
            raise serializers.ValidationError("Incorrect password")

        # popular passwords list
        if value in settings.PASSWORD_REQUIREMENTS['common_passwords'] or \
                (value.upper() == value and value.lower() in settings.PASSWORD_REQUIREMENTS['common_passwords']):
            raise serializers.ValidationError("Too common password")

        return value

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
        fields = ('email', 'first_name', 'last_name', 'subscribe', 'language')


class AccountUpdateSerializer(serializers.ModelSerializer):  # ModelSerializer
    class Meta:
        model = Account
        fields = ('first_name', 'last_name', 'subscribe', 'language')
