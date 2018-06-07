import copy
import json
import logging
import os
import shutil
import sys
import tempfile
from textwrap import dedent

import click
from flask import Flask, request, send_file
from pathlib2 import Path
from werkzeug.exceptions import BadRequest, NotFound, SecurityError

if sys.version_info[:2] == (2, 7):
    # noinspection PyCompatibility,PyUnresolvedReferences
    from httplib import HTTPConnection
elif sys.version_info[:2] in {(3, 4), (3, 5), (3, 6)}:  # Python 3.4 is on Ubuntu 14.04.
    # noinspection PyCompatibility,PyUnresolvedReferences
    from http.client import HTTPConnection

_logger = logging.getLogger(__name__)

UPDATE_PATH_PATTERN = '/{}/{}/update.json'


def write_json(path, data):
    """In Python 2.7, 3.4 and 3.5 json.dumps returns different types."""
    data_json = json.dumps(data, indent=2)
    if isinstance(data_json, bytes):
        path.write_bytes(data_json)
    else:
        path.write_text(data_json)


def read_json(path):
    """In Python 2.7, 3.4 and 3.5 json.loads accepts different types."""
    if str == bytes:
        return json.loads(path.read_bytes())
    else:
        return json.loads(path.read_text())


def collect_actual_data():
    base_url = 'updates.networkoptix.com'

    conn = HTTPConnection(base_url, port=80)
    conn.request('GET', '/updates.json')
    root_obj = json.loads(conn.getresponse().read().decode())

    path_to_update_obj = dict()
    for customization in root_obj:
        if customization == '__info':
            continue
        releases_obj = root_obj[customization]['releases']
        for release in releases_obj:
            build_num = releases_obj[release].split('.')[-1]
            update_path = UPDATE_PATH_PATTERN.format(customization, build_num)
            conn.request('GET', update_path)
            response = conn.getresponse()
            response_contents = response.read().decode()
            if response.status != 200:
                _logger.warning("Ignore %s: HTTP status code %d.", update_path, response.status)
                continue
            try:
                update_metadata = json.loads(response_contents)
            except ValueError:
                _logger.warning("Ignore %s: not a valid JSON.", update_path)
                continue
            path_to_update_obj[update_path] = update_metadata

    return root_obj, path_to_update_obj


def append_new_versions(root_obj, path_to_update_obj, new_versions):
    for customization_name, customization_obj in root_obj.items():
        # Searching for the update object with the highest build number for the current customization.
        new_update_obj = None
        highest_build = 0
        for path in path_to_update_obj.keys():
            path_parts = path.split('/')
            if customization_name == path_parts[1] and int(path_parts[2]) > highest_build:
                if 'packages' in path_to_update_obj[path]:
                    highest_build = int(path_parts[2])
                    new_update_obj = copy.deepcopy(path_to_update_obj[path])

        if not new_update_obj:
            continue

        for new_version in new_versions:
            customization_obj['releases'][new_version[0]] = new_version[1]
            new_update_obj['version'] = new_version[1]
            new_update_obj['cloudHost'] = new_version[2]

            def amend_file_info_obj(file_info_obj):
                # TODO: amend md5 and size as well
                # Amending file name. Changing only the version part of the file name.
                old_file_name_splits = file_info_obj['file'].split('.')
                new_version_splits = new_version[1].split('.')
                new_file_name_prefix = list(old_file_name_splits[0])
                new_file_name_prefix[-1] = new_version_splits[0]
                new_file_name_prefix = ''.join(new_file_name_prefix)
                new_file_name = '.'.join([
                    new_file_name_prefix,
                    new_version_splits[1], new_version_splits[2], new_version_splits[3],
                    'zip'])
                file_info_obj['file'] = new_file_name

            def amend_packages(update_obj, packages_name):
                for os_key, os_obj in update_obj[packages_name].items():
                    for file_key, file_obj in os_obj.items():
                        amend_file_info_obj(file_obj)

            amend_packages(new_update_obj, 'clientPackages')
            amend_packages(new_update_obj, 'packages')

            new_path_key = UPDATE_PATH_PATTERN.format(customization_name, new_version[1].split('.')[-1])
            path_to_update_obj[new_path_key] = new_update_obj

    return root_obj, path_to_update_obj


def save_data_to_files(data_dir, root_obj, path_to_update_obj):
    shutil.rmtree(str(data_dir), ignore_errors=True)
    data_dir.mkdir()
    write_json(data_dir / 'updates.json', root_obj)
    for key, value in path_to_update_obj.items():
        file_path = data_dir / key.lstrip('/')
        file_path.parent.mkdir(parents=True, exist_ok=True)
        write_json(file_path, value)


@click.group(help="Test updates server", epilog=dedent("""
    Replicates functionality of updates.networkoptix.com but
    appends new versions and cloud host information and
    corresponding specific updates links to the existing ones.
    You may specify versions to add as arguments of append_new_versions(...)."""))
@click.option(
    '-d', '--dir', '--data-dir',
    type=click.Path(file_okay=False, dir_okay=True, resolve_path=True),
    default=os.path.join(tempfile.gettempdir(), 'test_updates_server'),
    help="Directory to store downloaded and generated data and serve updates from.")
@click.pass_context
def main(ctx, data_dir):
    ctx.obj['data_dir'] = Path(data_dir)
    ctx.obj['dummy_file_path'] = ctx.obj['data_dir'] / 'dummy.zip'
    pass


@main.command(short_help="Collect data, add versions", help=dedent("""
    Collect actual data from updates.networkoptix.com, add versions and save to the data dir."""))
@click.pass_context
def generate(ctx):
    data_dir = ctx.obj['data_dir']
    dummy_file_path = ctx.obj['dummy_file_path']
    _logger.info('Loading and generating data. Be patient')
    root_obj, path_to_update_obj = collect_actual_data()
    new_versions = [('4.0', '4.0.0.21200', 'cloud-test.hdw.mx')]
    new_root, new_path_to_update_obj = append_new_versions(root_obj, path_to_update_obj, new_versions)
    save_data_to_files(data_dir, new_root, new_path_to_update_obj)
    if not dummy_file_path.exists():
        with dummy_file_path.open('wb') as f:
            f.seek(1024 * 1024 * 100)
            f.write(b'\0')


@main.command(short_help="Start HTTP server", help="Serve update metadata and, optionally, archives by HTTP.")
@click.option('--serve-update-archives/--no-serve-update-archives', default=True, show_default=True, help=dedent("""
    Server can serve update files requests. Pre-generated dummy file is given as a response.
    This option might be turned off to test how mediaserver deals with update files absence."""))
@click.option('--range-header', type=click.Choice(['support', 'ignore', 'err']), default='support', show_default=True)
@click.pass_context
def serve(ctx, serve_update_archives, range_header):
    data_dir = ctx.obj['data_dir']
    dummy_file_path = ctx.obj['dummy_file_path']

    app = Flask(__name__)

    @app.route('/<path:path>')
    def serve(path):
        path = data_dir.joinpath(path).resolve()
        if data_dir not in path.parents:
            raise SecurityError("Resolved path is outside of data dir.")
        if 'Range' in request.headers and range_header == 'err':
            raise BadRequest('Range header is not supported')
        if path.suffix == '.json':
            if path.exists():
                return send_file(str(path))
        elif path.suffix == '.zip' and serve_update_archives:
            update_path = path.with_name('update.json')
            if update_path.exists():
                update = read_json(update_path)
                for packages_key in {'packages', 'clientPackages'}:
                    for os_specific_packages in update[packages_key].values():
                        for package in os_specific_packages.values():
                            if package['file'] == path.name:
                                return send_file(
                                    str(dummy_file_path),
                                    as_attachment=True, attachment_filename=path.name,
                                    conditional=range_header == 'support')
        raise NotFound()

    app.run(port=8080)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    main(obj={})
