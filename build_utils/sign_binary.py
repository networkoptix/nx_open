#!/usr/bin/env python

import argparse
import requests
from requests.adapters import HTTPAdapter
from requests.packages.urllib3.util.retry import Retry

chunk_size = 1024 * 1024

DEFAULT_TIMEOUT = 120
DEFAULT_RETRIES = 10


def bool_to_str(value):
    return 'true' if value else 'false'


def sign_binary(
    url,
    file,
    output,
    customization,
    trusted_timestamping,
    timeout=DEFAULT_TIMEOUT,
    max_retries=DEFAULT_RETRIES
):

    params = {
        'customization': customization,
        'trusted_timestamping': bool_to_str(trusted_timestamping)
    }

    files = {
        'file': open(file, 'rb')
    }

    retries = Retry(
        total=max_retries,
        backoff_factor=0.1)

    session = requests.Session()
    session.mount(url, HTTPAdapter(max_retries=retries))
    try:
        r = session.post(url, params=params, files=files, timeout=timeout)
        if r.status_code != 200:
            print('ERROR: {}'.format(r.text))
            return r.status_code

        with open(output, 'wb') as fd:
            for chunk in r.iter_content(chunk_size=chunk_size):
                fd.write(chunk)

        return 0
    except requests.exceptions.ReadTimeout as e:
        print('ERROR: Connection to the signing server has timed out' +
              ' ({} seconds, {} retries) while signing {}'.format(timeout, max_retries, file))
        print(e)
        return 1
    except requests.exceptions.ConnectionError as e:
        print('ERROR: Connection to the signing server cannot be established ' +
              'while signing {}.'.format(file))
        print(e)
        return 2


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
        '--retries',
        help='Max retries count ({})'.format(DEFAULT_RETRIES),
        type=int,
        default=DEFAULT_RETRIES)
    parser.add_argument(
        '--timeout',
        help='Request timeout in seconds ({})'.format(DEFAULT_TIMEOUT),
        type=int,
        default=DEFAULT_TIMEOUT)
    args = parser.parse_args()

    return sign_binary(
        url=args.url,
        file=args.file,
        output=args.output if args.output else args.file,
        customization=args.customization,
        trusted_timestamping=args.trusted_timestamping,
        timeout=args.timeout,
        max_retries=args.retries)


if __name__ == '__main__':
    ret_code = main()
    exit(ret_code)
