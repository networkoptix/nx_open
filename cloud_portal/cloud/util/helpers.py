from django.conf import settings
from django.core.exceptions import ObjectDoesNotExist
from api.models import Account


def get_language_for_email(email):
    try:
        language = Account.objects.get(email=email).language
    except ObjectDoesNotExist:
        language = settings.DEFAULT_LANGUAGE

    if not language:
        language = settings.DEFAULT_LANGUAGE

    return language
