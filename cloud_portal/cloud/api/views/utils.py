from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from django.core.cache import cache
from rest_framework.permissions import AllowAny
from api.helpers.exceptions import handle_exceptions, api_success, require_params, APIRequestException, ErrorCodes
import datetime, logging
import json
import requests
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


def detect_language_by_request(request):
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

    if not lang or lang not in settings.LANGUAGES:  # not supported language
        lang = settings.DEFAULT_LANGUAGE  # return default
    return lang


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
def language(request):
    if request.method == 'GET':  # Get language for current user
        lang = detect_language_by_request(request)
        language_file = '/static/lang_' + lang + '/language.json'
        # Return: redirect to language.json file for selected language
        return redirect(language_file)
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


@api_view(['GET'])
@permission_classes((AllowAny, ))
@handle_exceptions
def downloads(request):
    customization = settings.CUSTOMIZATION
    cache_key = "downloads_" + customization
    if request.method == 'POST':  # clear cache on POST request - only for this customization
        cache.set(cache_key, False)
    downloads_json = cache.get(cache_key, False)
    if not downloads_json:
        # get updates.json
        updates_json = requests.get(settings.UPDATE_JSON)
        updates_json.raise_for_status()
        updates_json = updates_json.json()

        # find settings for customizations
        if customization not in updates_json:
            customization = 'default'
        updates_record = updates_json[customization]
        latest_release = updates_record['current_release']
        if not latest_release:  # Hack for new customizations
            latest_release = '3.0'
        latest_version = updates_record['releases'][latest_release]

        build_number = latest_version.split('.')[-1]
        updates_path = updates_record['updates_prefix']

        # get downloads.json for specific version
        downloads_path = updates_path + '/' + build_number + '/downloads.json'
        downloads_json = requests.get(downloads_path)

        # Check response result here
        if downloads_json.status_code != requests.codes.ok:
            # old or broken release - no downloads json
            # TODO: this is hardcode - remove it after release
            latest_version = updates_record['releases']['3.0']
            build_number = latest_version.split('.')[-1]        # Use the latest 3.0 public version
            downloads_path = updates_path + '/' + build_number + '/downloads.json'
            downloads_json = requests.get(downloads_path)
            pass

        downloads_json.raise_for_status()
        try:
            downloads_json = downloads_json.json()
        except:
            raise APIRequestException("Cant read downloads: " + downloads_path + " Code: " + str(downloads_json.status_code) +
                                      " Text:" + downloads_json.text, ErrorCodes.deserialization_error)

        downloads_json['releaseNotes'] = updates_record['release_notes']
        downloads_json['releaseUrl'] = updates_path + '/' + build_number + '/'
        # add release notes to downloads.json
        # evaluate file pathss
        # release_notes = updates_record['release_notes']

        cache.set(cache_key, json.dumps(downloads_json))
    else:
        downloads_json = json.loads(downloads_json)
    return Response(downloads_json)
