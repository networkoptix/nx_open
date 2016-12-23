from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from django.core.cache import cache
from rest_framework.permissions import AllowAny
from api.helpers.exceptions import handle_exceptions, api_success, require_params, APIRequestException, ErrorCodes
import datetime, logging
from cloud import settings
from django.shortcuts import redirect

logger = logging.getLogger(__name__)


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def visited_key(request):
    if request.method == 'GET':
        # Check cache value here
        if 'key' not in request.query_params:
            raise APIRequestException('Parameter key is missing', ErrorCodes.wrong_parameters,
                                      error_data=request.query_params)
        key = 'visited_key_' + request.query_params['key']
        value = cache.get(key, False)
    elif request.method == 'POST':
        # Save cache value here
        require_params(request, ('key',))
        key = 'visited_key_' + request.data['key']
        value = datetime.datetime.now().strftime('%c')

        logger.debug('visited: ' + key + ': ' + value)

        cache.set(key, value, settings.LINKS_LIVE_TIMEOUT)
    return Response({'visited': value})


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
def language(request):
    if request.method == 'GET':  # Get language for current user
        # 1. Try session value
        lang = request.session.get('language', False)

        # 2. Try cookie value
        if not lang:
            if 'language' in request.COOKIES:
                lang = request.COOKIES['language']

        # 3. Try account value
        if not lang and request.user.is_authenticated():
            lang = request.user.language

        # 4. Try ACCEPT_LANGUAGE header
        if not lang and 'HTTP_ACCEPT_LANGUAGE' in request.META:
            languages = request.META['HTTP_ACCEPT_LANGUAGE']
            languages = languages.split(';')[0]
            languages = languages.split(',')
            for l in languages:
                if l in settings.LANGUAGES:
                    lang = l
                    break
                if l.split('-')[0] in settings.LANGUAGES:
                    lang = l.split('-')[0]
                    break

        if not lang:  # not supported language
            lang = settings.DEFAULT_LANGUAGE  # return default

        language_file = '/static/lang_' + lang + '/language.json'
        return redirect(language_file)
        # Return: redirect to language.json file for selected language
        pass
    elif request.method == 'POST':
        require_params(request, ('language',))
        lang = request.data['language']

        # Save session value
        request.session['language'] = lang

        # Save account value
        if request.user.is_authenticated():
            request.user.language = lang
            request.user.save()

        response = Response({'language': lang})
        # Save cookie
        response.set_cookie('language', lang, 60 * 60 * 24 * 7)  # Cookie for one week
        return response
