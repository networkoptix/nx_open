from django.conf import settings
from django.core.exceptions import ObjectDoesNotExist
from api.models import Account
from django.core.cache import cache
from util.config import get_config
from cms.controllers.filldata import find_actual_value


def read_global_value(customization, value, version_id):
    from cms.models import Product
    product = Product.objects.get(name='cloud_portal')
    global_contexts = product.context_set.filter(is_global=True)
    record = None
    for context in global_contexts:
        records = context.datastructure_set.filter(name=value)
        if records.exists():
            record = records.last()
            break

    if not record:
        return None
    return find_actual_value(record, version=version_id)


def customization_cache(customization_name, value=None):
    data = cache.get(customization_name)
    if not data:
        from cms.models import Customization
        customization = Customization.objects.get(name=customization_name)
        custom_config = get_config(customization)

        version_id = customization.contentversion_set.latest('accepted_date').id \
            if customization.contentversion_set.exists() else 0
        data = {
            'version_id': version_id,
            'languages': customization.languages.values_list('code', flat=True),
            'default_language': customization.default_language.code,
            'mail_from_name': read_global_value(customization, '%MAIL_FROM_NAME%', version_id),
            'mail_from_email': read_global_value(customization, '%MAIL_FROM_EMAIL%', version_id),
            'portal_url': custom_config['cloud_portal']['url']
        }
        cache.set(customization_name, data)
        
    if value:
        return data[value] if value in data else None
    return data


def get_languages(customization=None):
    if not customization:
        customization = settings.CUSTOMIZATION

    return customization_cache(customization, 'default_language'), \
        customization_cache(customization, 'languages')


def detect_language_by_request(request):
    lang = None
    default_language, languages = get_languages()

    # 1. Try account value - top priority
    if request.user.is_authenticated():
        lang = request.user.language

    # 2. try session valie
    if not lang:
        lang = request.session.get('language', False)

    # 3. Try cookie value (saved in browser some time ago)
    if not lang:
        if 'language' in request.COOKIES:
            lang = request.COOKIES['language']

    # 4. Try ACCEPT_LANGUAGE header
    if not lang and 'HTTP_ACCEPT_LANGUAGE' in request.META:
        languages = request.META['HTTP_ACCEPT_LANGUAGE']
        languages = languages.split(';')[0]
        languages = languages.split(',')
        for l in languages:
            if l in languages:
                lang = l
                break

    if not lang or lang not in languages:  # not supported language
        lang = default_language  # return default
    return lang


def get_language_for_email(email, customization):
    default_language, languages = get_languages(customization)

    try:
        language = Account.objects.get(email=email).language
    except ObjectDoesNotExist:
        language = default_language

    if not language or language not in languages:
        language = default_language

    return language
