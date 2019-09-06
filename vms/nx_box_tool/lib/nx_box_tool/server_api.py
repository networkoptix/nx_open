import urllib.request
from urllib.parse import urlencode
from urllib.error import URLError
import base64
import json

from nx_box_tool.camera import Camera
from nx_box_tool.license import License


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

    def ping(self):
        try:
            response = urllib.request.urlopen(f"http://{self.ip}:{self.port}/api/ping")
            result = self.Response(response.code)

            if 200 <= response.code < 300:
                result.payload = json.loads(response.read())
        except URLError:
            return None

        return result

    def get_test_cameras(self):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/ec2/getCamerasEx")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        response = urllib.request.urlopen(request)
        result = self.Response(response.code)

        if 200 <= response.code < 300:
            result.payload = json.loads(response.read())

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

    def get_licenses(self):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/ec2/getLicenses")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        response = urllib.request.urlopen(request)

        result = self.Response(response.code)

        if 200 <= response.code < 300:
            result.payload = json.loads(response.read())
            return [
                License.parse(license_description['licenseBlock'])
                for license_description in result.payload
            ]

        return None

    def activate_license(self, license):
        request = urllib.request.Request(f"http://{self.ip}:{self.port}/ec2/addLicense")
        credentials = f"{self.user}:{self.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        request.add_header('Content-Type', 'application/json')
        request.get_method = lambda: 'POST'
        response = urllib.request.urlopen(
            request,
            data=urlencode({
                "key": license.serial,
                "licenseBlock": license.to_content()
            }).encode()
        )

        return 200 <= response.code < 300

