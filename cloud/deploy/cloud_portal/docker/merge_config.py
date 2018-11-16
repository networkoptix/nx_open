#!/usr/bin/env python

from __future__ import print_function

import io
import os
import sys

import json
import yaml

import base64

from functools import partial


def json_read(filename):
    with io.open(filename, encoding='utf8') as f:
        return json.load(f)


def json_write(filename, data):
    with io.open(filename, 'w', encoding='utf8') as f:
        json.dump(data, f, ensure_ascii=False)


def yaml_read(filename):
    with io.open(filename, encoding='utf8') as f:
        return yaml.safe_load(f)


def yaml_write(filename, data):
    with io.open(filename, 'w', encoding='utf8') as f:
        yaml.safe_dump(data, f, default_flow_style=False)


def detect_format(filename):
    """
    Returns reader/writer for a format based on file extension
    """
    formats = {
        'json': (json_read, json_write),
        'yaml': (yaml_read, yaml_write)
    }

    extension = os.path.splitext(filename)[1][1:]

    r,w = formats[extension]
    return partial(r, filename), partial(w, filename)


def merge_dictionaries(base_config, extra_config):
    for key, value in extra_config.items():
        if isinstance(value, dict):
            # get node or create one
            node = base_config.setdefault(key, {})
            merge_dictionaries(node, value)
        else:
            base_config[key] = value


def main(args):
    if len(args) != 1:
        print('Usage: merge_config [config_file.yaml|config_file.json]')
        print('Input is base64 encoded json')
        sys.exit(1)

    base_config_file, = args

    base_read, base_write = detect_format(base_config_file)
    base_config = base_read()

    extra_config_b64 = sys.stdin.read()
    extra_config_json = base64.decodestring(extra_config_b64.encode('ascii')).decode('utf-8')
    extra_config = json.loads(extra_config_json)

    merge_dictionaries(base_config, extra_config)

    base_write(base_config)


if __name__ == '__main__':
    main(sys.argv[1:])
