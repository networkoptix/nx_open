#!/usr/bin/python3

# Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Helper script to update trusted_certificates in the repository for all NX related services.

STORAGE_URL = 'https://certs.vmsproxy.com/manifest.json'
TARGET_DIR = 'trusted_certificates'

import os
import sys
import requests

def update_certs(url, target_dir):
    sys.stdout.write(f'Cleaning up target dir {target_dir}\n')
    for name in os.listdir(target_dir):
        path = os.path.join(target_dir, name)
        if os.path.isfile(path):
            sys.stdout.write(f'- Removed {name}\n')
            os.unlink(path)
    sys.stdout.write(f'Download from {url}')
    for folder in requests.get(url).json():
        if not folder['folder'].startswith('root'):
            sys.stdout.write(f'- Skip folder {folder['folder']}\n')
            continue
        sys.stdout.write(f'- Read folder {folder['folder']}\n')
        for cert in folder['files']:
            name = cert['name'][:-4] if cert['name'].endswith('.txt') else cert['name']
            path = os.path.join(target_dir, name)
            sys.stdout.write(f'-- Downloading {name} from {cert['url']}\n')
            with open(path, 'wb') as f:
                f.write(requests.get(cert['url']).content)


if __name__ == '__main__':
    update_certs(STORAGE_URL, os.path.join(os.path.dirname(os.path.realpath(__file__)), TARGET_DIR))
