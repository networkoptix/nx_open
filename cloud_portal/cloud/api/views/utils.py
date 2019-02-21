from rest_framework.decorators import api_view, permission_classes
from rest_framework.response import Response
from django.core.cache import caches
from rest_framework.permissions import AllowAny, IsAuthenticated
from api.helpers.exceptions import handle_exceptions, api_success, require_params, \
    APIRequestException, APIForbiddenException, APINotFoundException, ErrorCodes
import datetime, logging
import json
import re
import requests
from cloud import settings
from django.shortcuts import redirect

from cms.models import Customization, UserGroupsToCustomizationPermissions, customization_cache

logger = logging.getLogger(__name__)


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def visited_key(request):
    global_cache = caches['global']
    if request.method == 'GET':
        # Check cache value here
        if 'key' not in request.query_params:
            raise APIRequestException('Parameter key is missing', ErrorCodes.wrong_parameters,
                                      error_data=request.query_params)
        key = 'visited_key_' + request.query_params['key']
        value = global_cache.get(key, False)

        logger.debug('check visited: {0}: {1}'.format(key,value))

    elif request.method == 'POST':
        # Save cache value here
        require_params(request, ('key',))
        key = 'visited_key_' + request.data['key']
        value = datetime.datetime.now().strftime('%c')
        global_cache.set(key, value, settings.LINKS_LIVE_TIMEOUT)

        logger.debug('visited: {0}: {1}'.format(key,value))

    return Response({'visited': value})


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
def language(request):
    if request.method == 'GET':  # Get language for current user
        from util.helpers import detect_language_by_request
        lang = detect_language_by_request(request)
        language_file = '/static/lang_{0}/language.json'.format(lang)
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
    customization = Customization.objects.get(name=settings.CUSTOMIZATION)
    can_view_releases = UserGroupsToCustomizationPermissions.check_permission(request.user, customization.name,
                                                                              'api.can_view_release')
    if not customization.public_release_history and not can_view_releases:
        raise APIForbiddenException("Not authorized", ErrorCodes.forbidden)

    downloads_url = settings.DOWNLOADS_JSON.replace('{{customization}}', customization.name)
    downloads_json = requests.get(downloads_url)
    downloads_json.raise_for_status()
    downloads_json = downloads_json.json()

    return Response(downloads_json)


@api_view(['GET'])
@permission_classes((IsAuthenticated,))
@handle_exceptions
def download_build(request, build):
    # TODO: later we can check specific permissions
    customization = settings.CUSTOMIZATION
    if not UserGroupsToCustomizationPermissions.check_permission(request.user, customization, 'api.can_view_release') \
            and not Customization.objects.get(name=customization).public_release_history:
        raise APIForbiddenException("Not authorized", ErrorCodes.forbidden)

    if re.search(r'\D+', build):
        raise APINotFoundException("Invalid build number", ErrorCodes.bad_request)

    downloads_url = settings.DOWNLOADS_VERSION_JSON.replace('{{customization}}', customization).replace('{{build}}', build)
    downloads_json = requests.get(downloads_url)

    if downloads_json.status_code != 200:
        raise APINotFoundException("Build number does not exist", ErrorCodes.not_found, error_data=request.query_params)

    downloads_json = downloads_json.json()

    if 'releaseNotes' not in downloads_json:
        raise APINotFoundException("No downloads.json for this build", ErrorCodes.not_found, error_data=request.query_params)

    updates_json = requests.get(settings.UPDATE_JSON)
    updates_json.raise_for_status()
    updates_json = updates_json.json()

    # find settings for customizations
    if customization not in updates_json:
        logger.error('Customization not in updates.json: {0}. Ask Boris to fix that.'.format(customization))
        customization = 'default'

    updates_record = updates_json[customization]
    downloads_json['updatesPrefix'] = updates_record['updates_prefix']

    return Response(downloads_json)


@api_view(['GET', 'POST'])
@permission_classes((AllowAny, ))
@handle_exceptions
def downloads(request):
    global_cache = caches['global']
    customization = settings.CUSTOMIZATION
    customization_model = Customization.objects.get(name=settings.CUSTOMIZATION)
    if not customization_model.public_downloads and not request.user.is_authenticated:
        raise APIForbiddenException("Not authorized", ErrorCodes.not_authorized)
    cache_key = "downloads_" + customization
    if request.method == 'POST':  # clear cache on POST request - only for this customization
        global_cache.set(cache_key, False)
    downloads_json = global_cache.get(cache_key, False)
    if not downloads_json:
        # get updates.json
        updates_json = requests.get(settings.UPDATE_JSON)
        updates_json.raise_for_status()
        updates_json = updates_json.json()

        # find settings for customizations
        if customization not in updates_json:
            logger.error('Customization not in updates.json: {0}. Ask Boris to fix that.'.format(customization))
            return Response(None)
        updates_record = updates_json[customization]
        latest_version = updates_record['download_version'] if 'download_version' in updates_record else None

        # Fallback section for old structure and old versions
        if not latest_version or latest_version.startswith('2'):
            if latest_version and latest_version.startswith('2'):
                logger.error('No 3.0 downloadable release for customization: {0}. '
                             'Ask Boris to fix that.'.format(customization))
            else:
                logger.error('No download_version in updates.json for customization: {0}. '
                             'Ask Boris to fix that.'.format(customization))
            latest_release = None
            if 'current_release' in updates_record:
                latest_release = updates_record['current_release']
            if not latest_release:  # Hack for new customizations
                logger.error('No official release for customization: {0}. '
                             'Ask Boris to fix that.'.format(customization))
                latest_release = '3.0'
            if latest_release.startswith('2'):  # latest release is 2.* - fallback for 3.0
                latest_release = '3.0'
            if latest_release not in updates_record['releases']:
                logger.error('No 3.0 release for customization: {0}. Ask Boris to fix that.'.format(customization))
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

        global_cache.set(cache_key, json.dumps(downloads_json))
    else:
        downloads_json = json.loads(downloads_json)
    return Response(downloads_json)


@api_view(['GET'])
@permission_classes((AllowAny, ))
@handle_exceptions
def get_settings(request):
    customization = Customization.objects.get(name=settings.CUSTOMIZATION)

    support_link = customization_cache(settings.CUSTOMIZATION, 'support_link')

    settings_object = {
        'trafficRelayHost': settings.TRAFFIC_RELAY_HOST,
        'publicDownloads': customization.public_downloads,
        'publicReleases': customization.public_release_history,
        'supportLink': support_link
    }
    return Response(settings_object)
