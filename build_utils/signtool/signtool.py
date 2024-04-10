#!/usr/bin/env python

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import argparse
from dataclasses import dataclass
import logging
import os
from pathlib import Path
import random
import shutil
import subprocess
import sys
from typing import Dict, List
import yaml

DEFAULT_SIGN_TIMEOUT_S = 90
CERT_INFO_TIMEOUT_S = 30

logger = logging.getLogger(__name__)
working_dir = Path(__file__).parent.resolve()


@dataclass
class ExecuteCommandResult:
    stdout: bytes
    stderr: bytes
    returncode: int

    def __str__(self):
        text = str(self.returncode)
        if self.stdout:
            text += '\n' + self.stdout.decode().strip()
        if self.stderr:
            text += '\n' + self.stderr.decode().strip()
        return text

    def __bool__(self):
        return self.returncode == 0


def sign_file(
        in_file_path: Path,
        out_file_path: Path,
        config_file: Path,
        sign_timeout_s: int = 0) -> ExecuteCommandResult:

    with open(config_file, 'r') as f:
        config = yaml.safe_load(f)

    if out_file_path and out_file_path != in_file_path:
        shutil.copyfile(in_file_path, out_file_path)
        file_to_sign = out_file_path
    else:
        file_to_sign = in_file_path

    timestamp_servers = config.get('timestamp_servers')

    logger.info(
        f'Signing {in_file_path!s} using signtool.exe '
        f'({"trusted" if timestamp_servers else "no timestamp"}). Output file: {file_to_sign!s}')

    if timestamp_servers:
        timestamp_server = random.choice(timestamp_servers)
        logging.info(f'Using trusted timestamping server: {timestamp_server}')
    else:
        timestamp_server = None

    command = [
        'signtool',
        'sign',
        '/fd', 'sha256',
        '/v']
    if timestamp_server:
        command += ['/td', 'sha256', '/tr', timestamp_server]

    env_overload = os.environ.copy()
    if config.get('cloud'):
        logging.info(f'Using cloud certificate')
        api_token = config.get('api_token')
        client_certificate = (config_file.parent /  config.get('client_cert')).resolve()
        client_certificate_password = config.get('client_cert_password')
        env_overload["SM_API_KEY"] = api_token
        env_overload["SM_HOST"] = "https://clientauth.one.digicert.com"
        env_overload["SM_CLIENT_CERT_FILE"] = str(client_certificate)
        env_overload["SM_CLIENT_CERT_PASSWORD"] = client_certificate_password
        certificate = (config_file.parent / config.get('certificate')).resolve()
        keypair_alias = config.get('keypair_alias')
        command += ['/csp', 'DigiCert Signing Manager KSP']
        command += ['/f', str(certificate)]
        command += ['/kc', keypair_alias]
    else:
        certificate_file = (config_file.parent / config.get('file')).resolve()
        logging.info(f'Using certificate file: {certificate_file!s}')
        sign_password = config.get('password')
        print_certificate_info(certificate_file, sign_password)
        command += ['/f', str(certificate_file)]
        command += ['/p', sign_password]

    command += [str(file_to_sign)]

    return execute_command(command, timeout_s=sign_timeout_s, env=env_overload)


def execute_command(command: List[str], timeout_s: int = 0, env=None) -> ExecuteCommandResult:
    try:
        logging.debug(
            f'Running "{" ".join(command)}"'
            f'{" with timeout " + str(timeout_s) + " seconds" if timeout_s else ""}')
        execution_result = subprocess.run(
            command,
            capture_output=True,
            timeout=timeout_s,
            env=env)

    except FileNotFoundError as e:
        logging.warning(e)
        return ExecuteCommandResult(b'', b'File was not found', 1)

    except subprocess.TimeoutExpired:
        logging.warning(f'Timeout occurred, process {command[0]!r} killed')
        return ExecuteCommandResult(b'', b'Process killed by timeout', 2)

    return ExecuteCommandResult(
        execution_result.stdout,
        execution_result.stderr,
        execution_result.returncode)


def print_certificate_info(certificate_file: Path, password: str):
    if not certificate_file.exists():
        logging.warning(f'File {certificate_file!s} was not found')
        return

    command = ['certutil', '-dump', '-p', password, str(certificate_file)]
    process_result = execute_command(command, timeout_s=CERT_INFO_TIMEOUT_S)
    if process_result:
        logging.debug(process_result.stdout.decode().strip())
    else:
        logging.warning(str(process_result))


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '-f', '--file', help='Path to file for signing', type=Path, required=True)
    parser.add_argument(
        '-o', '--output', help='File with signature', type=Path, required=False, default=None)
    parser.add_argument('-c', '--config', type=Path, required=True, help='Tool configuration file')
    parser.add_argument(
        '--sign-timeout',
        help=f'Signing timeout in seconds ({DEFAULT_SIGN_TIMEOUT_S})',
        type=int,
        default=DEFAULT_SIGN_TIMEOUT_S)

    params_group = parser.add_mutually_exclusive_group()
    params_group.add_argument(
        '--verbose', '-v',
        action='store_const', const=logging.DEBUG, default=logging.INFO,
        dest='log_level',
        help="Be verbose.")

    args = parser.parse_args()

    logging.basicConfig(level=args.log_level, format='%(levelname)-8s %(message)s')

    in_file_path = (working_dir / args.file).resolve()
    out_file_path = (working_dir / args.output).resolve() if args.output else None
    result = sign_file(
        in_file_path=in_file_path,
        out_file_path=out_file_path,
        config_file=args.config.resolve(),
        sign_timeout_s=args.sign_timeout)

    if result:
        print(f'File {in_file_path!s} was successfully signed.')
    else:
        sys.exit(f'FATAL ERROR: {in_file_path!s} was not signed.')


if __name__ == '__main__':
    main()
