import base64
import json
import urllib.request

from vms_benchmark.utils.catch_http_errors import catch_http_errors


# TODO: Refactor this class. It should use class ServerApi instead of use urllib directly.
class Camera:
    def __init__(self, api, id, name, mac):
        self.api = api
        self.id = id
        self.name = name
        self.mac = mac

    @catch_http_errors
    def enable_recording(self, highStreamFps, enable=True):
        request = urllib.request.Request(f"http://{self.api.ip}:{self.api.port}/ec2/saveCameraUserAttributes")
        credentials = f"{self.api.user}:{self.api.password}"
        encoded_credentials = base64.b64encode(credentials.encode('ascii'))
        request.add_header('Authorization', 'Basic %s' % encoded_credentials.decode("ascii"))
        request.add_header('Content-Type', 'application/json')
        request.get_method = lambda: 'POST'
        scheduleTasks = [
            {
                "startTime": 0,
                "endTime": 86400,
                "dayOfWeek": day_of_week,
                "fps": highStreamFps,
                "recordingType": "RT_Always",
                "streamQuality": "normal",
            }
            for day_of_week in range(1, 8)
        ]

        if enable:
            response = self.api.post_request(request, data=json.dumps({
                "cameraId": self.id,
                "scheduleEnabled": enable,
                "scheduleTasks": scheduleTasks,
            }).encode())
        else:
            response = self.api.post_request(request, data=json.dumps({
                "cameraId": self.id,
                "scheduleEnabled": enable,
            }).encode())

        if 200 <= response.code < 300:
            return True

        return False

    def disable_recording(self):
        return self.enable_recording(False)
