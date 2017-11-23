from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from django.core.cache import cache
from rest_framework.permissions import AllowAny, IsAuthenticated
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


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
def language(request):
    if request.method == 'GET':  # Get language for current user
        from util.helpers import detect_language_by_request
        lang = detect_language_by_request(request)
        language_file = '/static/lang_' + lang + '/language.json'
        # Return: redirect to language.json file for selected language
        response = redirect(language_file)

        request.session['language'] = lang
        response.set_cookie('language', lang, 60 * 60 * 24 * 7)  # Cookie for one week
        return response
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
@permission_classes((IsAuthenticated, ))
@handle_exceptions
def downloads_history(request):
    # TODO: later we can check specific permissions
    customization = settings.CUSTOMIZATION
    cache_key = "downloads_history_" + customization
    downloads_json = cache.get(cache_key, False)
    if not downloads_json:
        downloads_url = settings.DOWNLOADS_JSON.replace('{{customization}}', customization)
        downloads_json = requests.get(downloads_url)
        downloads_json.raise_for_status()
        downloads_json = downloads_json.json()
        cache.set(cache_key, json.dumps(downloads_json))
    else:
        downloads_json = json.loads(downloads_json)
    return Response(downloads_json)


@api_view(['GET'])
@permission_classes((IsAuthenticated,))
@handle_exceptions
def download_build(request, build):
    # TODO: later we can check specific permissions
    customization = settings.CUSTOMIZATION
    downloads_url = settings.DOWNLOADS_VERSION_JSON.replace('{{customization}}', customization).replace('{{build}}', build)
    downloads_json = requests.get(downloads_url)
    downloads_json.raise_for_status()
    return Response(downloads_json.json())


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def downloads(request):
    customization = settings.CUSTOMIZATION
    cache_key = "downloads_" + customization
    if request.method == 'POST':  # clear cache on POST request - only for this customization
        cache.set(cache_key, False)
        cache.set("all_downloads_" + customization, False)
    downloads_json = cache.get(cache_key, False)
    if not downloads_json:
        # get updates.json
        updates_json = requests.get(settings.UPDATE_JSON)
        updates_json.raise_for_status()
        updates_json = updates_json.json()

        # find settings for customizations
        if customization not in updates_json:
            logger.error('Customization not in updates.json: ' + customization + '. Ask Boris to fix that.')
            customization = 'default'
        updates_record = updates_json[customization]
        latest_version = updates_record['download_version'] if 'download_version' in updates_record else None

        # Fallback section for old structure and old versions
        if not latest_version or latest_version.startswith('2'):
            if latest_version and latest_version.startswith('2'):
                logger.error('No 3.0 downloadable release for customization: ' + customization +
                             '. Ask Boris to fix that')
            else:
                logger.error('No download_version in updates.json for customization: ' + customization +
                             '. Ask Boris to fix that.')
            latest_release = None
            if 'current_release' in updates_record:
                latest_release = updates_record['current_release']
            if not latest_release:  # Hack for new customizations
                logger.error('No official release for customization: ' + customization + '. Ask Boris to fix that.')
                latest_release = '3.0'
            if latest_release.startswith('2'):  # latest release is 2.* - fallback for 3.0
                latest_release = '3.0'
            if latest_release not in updates_record['releases']:
                logger.error('No 3.0 release for customization: ' + customization + '. Ask Boris to fix that')
                return Response(None)
            latest_version = updates_record['releases'][latest_release]
        # End of fallback section for old structure and old versions

        build_number = latest_version.split('.')[-1]
        updates_path = updates_record['updates_prefix']

        # get downloads.json for specific version. If get there - version is at least 3.0, so downloads.json is present
        downloads_path = updates_path + '/' + build_number + '/downloads.json'
        downloads_result = requests.get(downloads_path)
        downloads_result.raise_for_status()
        downloads_json = downloads_result.json()
        downloads_json['releaseNotes'] = updates_record['release_notes']
        downloads_json['releaseUrl'] = updates_path + '/' + build_number + '/'
        # add release notes to downloads.json
        # evaluate file pathss
        # release_notes = updates_record['release_notes']

        cache.set(cache_key, json.dumps(downloads_json))
    else:
        downloads_json = json.loads(downloads_json)
    return Response(downloads_json)
