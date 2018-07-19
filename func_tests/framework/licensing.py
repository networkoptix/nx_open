import json
import requests
from itertools import chain

DNS = 'licensing.networkoptix.com'
TEST_SERVER_IP = '107.23.248.56'
TEST_SERVER_URL = 'http://nxlicensed.hdw.mx/nxlicensed'
SERVICE_ACCOUNT = ('service@networkoptix.com', 'ARuMWFI3lnHP')


class Error(Exception):
    def __init__(self, msg):
        self.msg = msg

    def __str__(self):
        return str(self.msg)


# TODO: This code is just taken from Ivan Vigasin, better to refactor to meat local standards.
class ServerApi(object):
    def __init__(self, base_url=TEST_SERVER_URL, auth=SERVICE_ACCOUNT, login=True):
        self.base_url = base_url
        self.auth = auth
        self.session = requests.Session()
        if login:
            self._login()

    def __del__(self):
        self.session.close()

    def _login(self):
        url = self.base_url + '/checklogin.php'

        r = self.session.get(url)
        csrftoken = 'removeme'
        if 'csrftoken' in r.cookies:
            csrftoken = r.cookies['csrftoken']
        r = self.session.post(url, data=dict(zip(('email', 'password', 'csrfmiddlewaretoken'), chain(self.auth, (csrftoken,)))), headers=dict(Referer=url))
        if 'csrftoken' in self.session.cookies:
            self.session.headers.update({'X-CSRFToken': self.session.cookies['csrftoken']})
        assert r.status_code == 200, "Can't log in to licensing server"

    def deactivate(self, license_keys, autodeact_reason='1', new_hwid='',
                   integrator='AutoTest Integrator', end_user='AutoTest End User', mode='deactivate'
                   ):
        data = {
            'mode': mode,
            'license_keys[]': license_keys,
            'autodeact_reason': autodeact_reason,
            'new_hwid': new_hwid,
            'integrator': integrator,
            'end_user': end_user
        }

        text = self.session.post(self.base_url + '/autodeact.php?format=json', data=data).text
        print(text)
        return json.loads(text)

    def generate(self, name='Auto Test', company='Network Optix',
                 order_type='test', order_id='TEST-SB', authorized_by='1',
                 brand='hdwitness', license_type='digital',
                 trial_days=0, n_packs=1, n_cameras=1, replacements=[]):
        """Generate license key"""

        data = {
            'NAME': name,
            'COMPANY2': company,
            'ORDERTYPE': order_type,
            'ORDERID2': order_id,
            'AUTHORIZEDBY': authorized_by,
            'BRAND2': brand,
            'CLASS2': license_type,
            'TRIALDAYS2': trial_days,
            'NUMPACKS': n_packs,
            'QUANTITY2': n_cameras,
            'REPLACEMENT[]': replacements
        }

        text = self.session.post(self.base_url + '/genkey.php?format=json', data=data).text
        try:
            dict = json.loads(text)
            if 'status' in dict and dict['status'] == 'error':
                raise Error(dict['message'])
            return [x['key'] for x in dict['items']]
        except ValueError:
            raise Error(text)

    def disable(self, license_key):
        data = {
            'MODE': 'disable',
            'KEY': license_key
        }
        text = self.session.post(self.base_url + "/disable.php", data=data).text
        return text

    def info(self, license_key):
        data = {
            'KEY': license_key
        }

        url = self.base_url + "/do_getinfo.php"
        text = self.session.post(url, data=data, headers=dict(Referer=url)).text
        return json.loads(text)

    def disable_info(self, license_key):
        data = {
            'MODE': 'info',
            'KEY': license_key
        }
        text = self.session.post(self.base_url + "/disable.php", data=data).text
        return json.loads(text)

    def get_activation_report(self, date_from, date_to):
        data = {
            'from': date_from,
            'to': date_to
        }
        text = self.session.post(self.base_url + "/do_activation_report.php", data=data).text
        return text
