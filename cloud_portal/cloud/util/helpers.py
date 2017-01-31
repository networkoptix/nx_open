from django.conf import settings
from django.core.exceptions import ObjectDoesNotExist
from api.models import Account


def get_language_for_email(email, languages=None):
    if not languages:
        languages = settings.LANGUAGES
    default_language = languages[0]

    try:
        language = Account.objects.get(email=email).language
    except ObjectDoesNotExist:
        language = default_language

    if not language or language not in languages:
        language = default_language

    return language
