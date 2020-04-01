#!/usr/bin/env python

import argparse
import requests
from requests.adapters import HTTPAdapter
from requests.packages.urllib3.util.retry import Retry

chunk_size = 1024 * 1024

DEFAULT_SIGN_TIMEOUT = 90
DEFAULT_REQUEST_TIMEOUT = 180
DEFAULT_RETRIES = 10
assert DEFAULT_SIGN_TIMEOUT < DEFAULT_REQUEST_TIMEOUT


def bool_to_str(value):
    return 'true' if value else 'false'


def sign_binary(
    url,
    file,
    output,
    customization,
    trusted_timestamping,
    sign_timeout=DEFAULT_SIGN_TIMEOUT,
    request_timeout=DEFAULT_REQUEST_TIMEOUT,
    max_retries=DEFAULT_RETRIES
):
    if request_timeout <= sign_timeout:
        print('ERROR: Sign timeout ({}) must be less than request timeout ({})'.format(
            sign_timeout, request_timeout))
        return

    params = {
        'customization': customization,
        'trusted_timestamping': bool_to_str(trusted_timestamping),
        'sign_timeout': sign_timeout
    }

    last_status_code = 0
    for current_try in range(1, max_retries + 1):
        print('Signing file {}'.format(file))
        files = {
            'file': open(file, 'rb')
        }
        retries = Retry(
            total=max_retries,
            backoff_factor=0.1)
        session = requests.Session()
        session.mount(url, HTTPAdapter(max_retries=retries))
        try:
            r = session.post(url, params=params, files=files, timeout=request_timeout)
            if r.status_code == 200:
                with open(output, 'wb') as fd:
                    for chunk in r.iter_content(chunk_size=chunk_size):
                        fd.write(chunk)
                return 0
            else:
                print('ERROR: {}'.format(r.text))
                last_status_code = r.status_code

        except requests.exceptions.ReadTimeout as e:
            print('ERROR: Connection to the signing server has timed out' +
                  f' ({request_timeout} seconds, {max_retries} retries) ' +
                  f'while signing {file}')
            print(e)
            last_status_code = 1
        except requests.exceptions.ConnectionError as e:
            print('ERROR: Connection to the signing server cannot be established ' +
                  f'while signing {file}')
            print(e)
            last_status_code = 2
        except requests.exceptions.ChunkedEncodingError as e:
            print('ERROR: Connection to the signing server was broken ' +
                  f'while signing {file}')
            print(e)
            last_status_code = 3
        except Exception as e:
            print(f'ERROR: Unexpected exception while signing {file}')
            print(e)
            last_status_code = 4
        print('Trying again')

    print('ERROR: Too max retries failed. Status code {}'.format(last_status_code))
    return last_status_code


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-u', '--url', help='Signing server url', required=True)
    parser.add_argument('-f', '--file', help='Source file path', required=True)
    parser.add_argument(
        '-o', '--output',
        help='Target file path. Source file is replaced if omitted.')
    parser.add_argument('-c', '--customization', help='Selected customization', required=True)
    parser.add_argument(
        '-t', '--trusted-timestamping',
        action='store_true',
        help='Trusted timestamping')
    parser.add_argument(
        '--sign-timeout',
        help='Signing timeout in seconds ({}). Must be less than request timeout.'.format(
            DEFAULT_SIGN_TIMEOUT),
        type=int,
        default=DEFAULT_SIGN_TIMEOUT)
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
    args = parser.parse_args()

    return sign_binary(
        url=args.url,
        file=args.file,
        output=args.output if args.output else args.file,
        customization=args.customization,
        trusted_timestamping=args.trusted_timestamping,
        sign_timeout=args.sign_timeout,
        request_timeout=args.request_timeout,
        max_retries=args.retries)


if __name__ == '__main__':
    ret_code = main()
    exit(ret_code)
