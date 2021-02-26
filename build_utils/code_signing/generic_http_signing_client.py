import argparse
import requests
import sys
from requests.adapters import HTTPAdapter
from requests.packages.urllib3.util.retry import Retry


DEFAULT_REQUEST_TIMEOUT = 180
DEFAULT_RETRIES = 10
chunk_size = 1024 * 1024


class GenericHttpSigningClient():
    request_timeout = DEFAULT_REQUEST_TIMEOUT
    retries = DEFAULT_RETRIES
    url = None
    file = None
    output = None

    def __init__(self, path):
        self.path = path

    def setup_common_arguments(self, parser):
        parser.add_argument('-u', '--url', help='Signing server url', required=True)
        parser.add_argument('-f', '--file', help='Source file path', required=True)
        parser.add_argument('-o', '--output', help='Output file path.', required=True)
        parser.add_argument(
            '--retries',
            help='Max retries count ({})'.format(DEFAULT_RETRIES),
            type=int,
            default=DEFAULT_RETRIES)
        parser.add_argument(
            '--request-timeout',
            help='Request timeout in seconds ({}). Must be greater than sign timeout'.format(
                DEFAULT_REQUEST_TIMEOUT),
            type=int,
            default=DEFAULT_REQUEST_TIMEOUT)

    def load_arguments(self, args):
        self.url = f'{args.url}/{self.path}'
        self.file = args.file
        self.output = args.output
        self.request_timeout = args.request_timeout
        self.retries = args.retries

    def init_from_commmand_line(self):
        parser = argparse.ArgumentParser()
        self.setup_common_arguments(parser)
        args = parser.parse_args()
        self.load_arguments(args)

    def send_file(self, params={}):
        last_status_code = 0
        for current_try in range(1, self.retries + 1):
            retries = Retry(
                total=self.retries,
                backoff_factor=0.1)
            session = requests.Session()
            session.mount(self.url, HTTPAdapter(max_retries=retries))
            # An additional redirection check to avoid repeating a file transfer.
            response = session.get(self.url)
            if response.history:
                if len(response.history) > 1:
                    raise Exception("Too many redirects. Please check the nginx configuration.")
                elif response.history[0].status_code not in [301, 307, 308]:
                    raise Exception(
                        "Unexpected redirect: {} {}. Please, check configuration".format(
                            response.history[0].status_code,
                            response.history[0].reason
                        )
                    )
                else:
                    self.url = response.url
            try:
                with open(self.file, 'rb') as file_handle:
                    r = session.post(
                        self.url,
                        params=params,
                        files={'file': file_handle},
                        timeout=self.request_timeout)

                if r.status_code == 200:
                    with open(self.output, 'wb') as fd:
                        for chunk in r.iter_content(chunk_size=chunk_size):
                            fd.write(chunk)
                    return 0
                else:
                    print(f'ERROR: {r.text}', file=sys.stderr)
                    last_status_code = r.status_code

            except requests.exceptions.ReadTimeout as e:
                print('ERROR: Connection to the signing server has timed out'
                      + f' ({self.request_timeout} seconds, {self.max_retries} retries) '
                      + f'while signing {self.file}',
                      file=sys.stderr)
                print(e, file=sys.stderr)
                last_status_code = 1
            except requests.exceptions.ConnectionError as e:
                print('ERROR: Connection to the signing server cannot be established '
                      + f'while signing {self.file}',
                      file=sys.stderr)
                print(e, file=sys.stderr)
                last_status_code = 2
            except requests.exceptions.ChunkedEncodingError as e:
                print(f'ERROR: Connection to the signing server was broken {self.file}',
                      file=sys.stderr)
                print(e, file=sys.stderr)
                last_status_code = 3
            print(f'Trying to sign {self.file} again', file=sys.stderr)

        print(f'ERROR: Too max retries failed. Status code {last_status_code}', file=sys.stderr)
        return last_status_code
