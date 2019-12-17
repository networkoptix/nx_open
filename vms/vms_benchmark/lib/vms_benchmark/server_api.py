import base64
import json
import logging
import sys
import urllib.request
from collections import namedtuple
from typing import List
from urllib.error import URLError
from urllib.parse import urlencode

from vms_benchmark import exceptions
from vms_benchmark.camera import Camera
from vms_benchmark.license import License

Response = namedtuple('Response', ['code', 'body'])


def _catch_http_errors(action):
    def result_func(*args, **kwargs):
        try:
            return action(*args, **kwargs)
        except urllib.error.HTTPError as e:
            if e.code == 401 or e.code == 403:
                raise exceptions.ServerApiError(
                    f"Server failed to execute API request: insufficient VMS user permissions (HTTP code {e.code}).",
                    original_exception=e
                )
            if e.code == 400 or e.code == 404 or e.code == 406:
                raise exceptions.ServerApiError(
                    f"Server failed to execute API request: Server internal error (HTTP code {e.code}).",
                    original_exception=e
                )
            if 500 <= e.code <= 599:
                raise exceptions.ServerApiError(
                    f"Server failed to execute API request: Server internal error (HTTP code {e.code}).",
                    original_exception=e
                )
            raise exceptions.ServerApiError(e)
    return result_func


class ServerApi:
    class Response:
        def __init__(self, code):
            self.code = code
            self.payload = None

    def __init__(self, ip, port, user=None, password=None):
        self.ip = ip
        self.port = port
        self.user = user
        self.password = password

    @staticmethod
    def get_request(url_or_request):
        if isinstance(url_or_request, urllib.request.Request):
            url = url_or_request.full_url
        else:
            url = url_or_request
        logging.info(f"Sending HTTP request GET to Server:\n    {url}")
        response = urllib.request.urlopen(url_or_request)
        return ServerApi._read_response(response)

    @staticmethod
    def post_request(request, data):
        logging.info(f"Sending HTTP POST request to Server:\n    {request.full_url}\n"
            f"    with data\n    {data}")
        response = urllib.request.urlopen(request, data=data)
        return ServerApi._read_response(response)

    @staticmethod
    def _read_response(response):
        body = response.read()
        logging.info(f"Got HTTP response {response.code}:\n    {body!r}")
        return Response(response.code, body)

    def ping(self):
        try:
            response = self.get_request(f"http://{self.ip}:{self.port}/api/ping")
            result = self.Response(response.code)

            if 200 <= response.code < 300:
                result.payload = json.loads(response.body)
        except URLError:
            return None
        return result

    @_catch_http_errors
    def get_module_information(self):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/api/moduleInformationAuthenticated")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        response = self.get_request(request)

        result = self.Response(response.code)
        if 200 <= response.code < 300:
            result.payload = json.loads(response.body)

            if result.payload.get('error', 1) != '0' or 'reply' not in result.payload:
                return None

            return result.payload['reply']

    @_catch_http_errors
    def get_server_id(self):
        module_information = self.get_module_information()

        if module_information is None:
            raise exceptions.ServerApiError(message="Unable to get module information.")

        server_id_raw = module_information.get('id', '{00000000-0000-0000-0000-000000000000}')
        server_id = server_id_raw[1:-1] if server_id_raw[0] == '{' and server_id_raw[-1] == '}' else server_id_raw

        return server_id

    @_catch_http_errors
    def check_authentication(self):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/api/moduleInformationAuthenticated")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        self.get_request(request)

    @_catch_http_errors
    def get_test_cameras_all(self):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/ec2/getCamerasEx")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        response = self.get_request(request)
        result = self.Response(response.code)

        if 200 <= response.code < 300:
            result.payload = json.loads(response.body)

            cameras = [
                Camera(
                    api=self,
                    id=camera_description['id'][1:-1],
                    name=camera_description['name'],
                    mac=camera_description['mac']
                )
                for camera_description in result.payload
                if camera_description['mac'][:5] == '92-61'
            ]

            return cameras

        return None

    @_catch_http_errors
    def get_test_cameras(self) -> List[Camera]:
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/ec2/getCamerasEx")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        response = self.get_request(request)
        result = self.Response(response.code)

        if 200 <= response.code < 300:
            result.payload = json.loads(response.body)

            cameras = [
                Camera(
                    api=self,
                    id=camera_description['id'][1:-1],
                    name=camera_description['name'],
                    mac=camera_description['mac']
                )
                for camera_description in result.payload
                if camera_description['status'] in ['Online', 'Recording'] and camera_description['mac'][:5] == '92-61'
            ]

            return cameras

        return None

    @_catch_http_errors
    def get_licenses(self):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/ec2/getLicenses")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        response = self.get_request(request)

        result = self.Response(response.code)

        if 200 <= response.code < 300:
            result.payload = json.loads(response.body)
            return [
                License.parse(license_description['licenseBlock'])
                for license_description in result.payload
            ]

        return None

    @_catch_http_errors
    def get_storage_spaces(self):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/api/storageSpace")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        response = self.get_request(request)

        if 200 <= response.code < 300:
            json_reply = json.loads(response.body)

            if (
                json_reply['error'] != '0' or
                not isinstance(json_reply['reply'], dict) or
                not isinstance(json_reply['reply']['storages'], list)
            ):
                return None

            return json_reply['reply']['storages']

        return None

    @_catch_http_errors
    def get_events(self, ts_from):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/api/getEvents?from={ts_from}")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        response = self.get_request(request)

        result = self.Response(response.code)

        if 200 <= response.code < 300:
            result.payload = json.loads(response.body)
            if int(result.payload['error']) != 0:
                return None
            return result.payload['reply']

        return None

    @_catch_http_errors
    def get_statistics(self):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/api/statistics")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        response = self.get_request(request)

        result = self.Response(response.code)

        if 200 <= response.code < 300:
            result.payload = json.loads(response.body)
            if int(result.payload['error']) != 0 or 'reply' not in result.payload:
                return None
            return result.payload['reply']['statistics']

        return None

    @_catch_http_errors
    def add_cameras(self, hostname, count):
        from pprint import pformat
        cameras = []
        for i in range(count):
            request = urllib.request.Request(f"http://{self.ip}:{self.port}/ec2/saveCamera")
            credentials = f"{self.user}:{self.password}"
            encoded_credentials = base64.b64encode(credentials.encode('ascii'))
            request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
            request.add_header('Content-Type', 'application/json')
            request.get_method = lambda: 'POST'
            mac = f'92-61-00-00-00-{i + 1:02X}'
            name = 'TestCameraLive'
            data = {
                'status': 'Online',
                'mac': mac,
                'physicalId': mac,
                'name': name,
                'model': name,
                'url': f'tcp://{hostname}:4985/{mac}',
                'typeId': '{f9c03047-72f1-4c04-a929-8538343b6642}',
            }
            response = self.post_request(
                request,
                data=json.dumps(data).encode('ascii')
            )
            response_data = json.loads(response.body)
            logging.info(pformat(response_data))
            cameras.append(Camera(
                api=self,
                id=response_data['id'].strip('{}'),
                name=name,
                mac=mac,
            ))
        return cameras

    @_catch_http_errors
    def remove_camera(self, camera_id):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/ec2/removeResource")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        request.add_header('Content-Type', 'application/json')
        request.get_method = lambda: 'POST'
        response = self.post_request(
            request,
            data=json.dumps({
                "id": camera_id
            }).encode('ascii')
        )

        return 200 <= response.code < 300

    @_catch_http_errors
    def activate_license(self, license):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/ec2/addLicense")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        request.add_header('Content-Type', 'application/json')
        request.get_method = lambda: 'POST'
        response = self.post_request(
            request,
            data=urlencode({
                "key": license.serial,
                "licenseBlock": license.to_content()
            }).encode()
        )

        return 200 <= response.code < 300

    @_catch_http_errors
    def get_archive_start_time_ms(self, camera_id: str) -> int:
        request = urllib.request.Request(
            f"http://{self.ip}:{self.port}" +
            f"/ec2/recordedTimePeriods?cameraId={camera_id}"
        )
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        response = self.get_request(request)

        result = self.Response(response.code)

        if not (200 <= response.code < 300):
            raise exceptions.ServerApiError(
                "Unable to request recorded periods from the Server: "
                f"/ec2/recordedTimePeriods returned HTTP code {response.code}.")

        try:  # Any json deserialization errors and missing json object keys will be caught.
            result.payload = json.loads(response.body)
            error_code = result.payload['error']
            error_string = result.payload['errorString']
            if int(error_code) != 0:
                raise exceptions.ServerApiError(  # Will be caught below in this method.
                    f'API method reported error {error_code}: {error_string}')
            reply: list = result.payload['reply']
            min_start_time_ms = sys.maxsize
            for server in reply:
                periods: list = server['periods']
                for period in periods:
                    start_time_ms = int(period['startTimeMs'])
                    if start_time_ms < min_start_time_ms:
                        min_start_time_ms = start_time_ms
            if min_start_time_ms == sys.maxsize:
                raise exceptions.ServerApiError(  # Will be caught below in this method.
                    f'No recorded periods returned.')

            logging.info(f'Archive start time for camera {camera_id}: {min_start_time_ms} ms.')
            return min_start_time_ms
        except Exception:
            logging.exception('Exception while parsing the response of /ec2/recordedTimePeriods:')
            raise exceptions.ServerApiError(
                "Unable to request recorded periods from the Server: "
                f"Invalid response of /ec2/recordedTimePeriods.")
