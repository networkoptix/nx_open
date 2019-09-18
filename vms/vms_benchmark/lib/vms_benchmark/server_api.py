import urllib.request
from collections import namedtuple
from urllib.parse import urlencode
from urllib.error import URLError
import base64
import json
import logging

from vms_benchmark import exceptions
from vms_benchmark.camera import Camera
from vms_benchmark.license import License


Response = namedtuple('Response', ['code', 'body'])


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
        logging.info(f"Sending HTTP POST request to Server:\n    {request.full_url}\n    with data\n    {data}")
        response = urllib.request.urlopen(request, data=data)
        return ServerApi._read_response(response)

    @staticmethod
    def _read_response(response):
        body = response.read()
        logging.info(f"Got HTTP response {response.code}:\n    {body}")
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

    def check_authentication(self):
        try:
            request = urllib.request.Request(f"http://{self.ip}:{self.port}/api/moduleInformationAuthenticated")
            credentials = f"{self.user}:{self.password}"
            encoded_credentials = base64.b64encode(credentials.encode('ascii'))
            request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
            self.get_request(request)
        except urllib.error.HTTPError as e:
            if e.code == 401:
                raise exceptions.ServerApiError(
                    "API of Server is not working: "
                    "authentication was not successful, "
                    "check vmsUser and vmsPassword in vms_benchmark.conf.")
            raise

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

    def get_test_cameras(self):
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

    def get_storages(self):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/ec2/getStorages")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        response = self.get_request(request)

        if 200 <= response.code < 300:
            return json.loads(response.body)

        return None

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

