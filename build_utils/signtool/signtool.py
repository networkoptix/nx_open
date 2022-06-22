#!/usr/bin/env python

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import argparse
from dataclasses import dataclass
import logging
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
        trusted_timestamping: bool = False,
        sign_timeout_s: int = 0) -> ExecuteCommandResult:

    config_file = (working_dir / 'config' / 'config.yaml').resolve()
    with open(config_file, 'r') as f:
        config = yaml.safe_load(f)

    result = _sign_with_signtool(
        unsigned_file=in_file_path,
        signed_file=out_file_path,
        config=config,
        trusted_timestamping=trusted_timestamping,
        timeout_s=sign_timeout_s)

    if not result:
        logging.error(f'Signing tool failed: {result}')

    return result


def _sign_with_signtool(
        unsigned_file: Path,
        signed_file: Path,
        config: Dict[str, str],
        trusted_timestamping: bool,
        timeout_s: int) -> ExecuteCommandResult:

    if signed_file and signed_file != unsigned_file:
        shutil.copyfile(unsigned_file, signed_file)
        file_to_sign = signed_file
    else:
        file_to_sign = unsigned_file

    logger.info(
        f'Signing {unsigned_file!s} using signtool.exe '
        f'({"trusted" if trusted_timestamping else "no timestamp"}). Output file: '
        f'{file_to_sign}')

    timestamp_server = None
    if trusted_timestamping:
        timestamp_servers = config.get('timestamp_servers')
        timestamp_server = random.choice(timestamp_servers)
        logging.info(f'Using trusted timestamping server: {timestamp_server}')

    certificate_file = (working_dir / 'certs' / config.get('file')).resolve()
    sign_password = config.get('password')

    logging.info(f'Using certificate file: {certificate_file}')
    print_certificate_info(certificate_file, sign_password)

    command = [
        'signtool',
        'sign',
        '/fd', 'sha256',
        '/v']
    if timestamp_server:
        command += ['/td', 'sha256', '/tr', timestamp_server]
    command += ['/f', str(certificate_file)]
    command += ['/p', sign_password]
    command += [str(file_to_sign)]

    return execute_command(command, timeout_s=timeout_s)


def execute_command(command: List[str], timeout_s: int = 0) -> ExecuteCommandResult:
    try:
        logging.debug(
            f'Running "{" ".join(command)}"'
            f'{" with timeout " + str(timeout_s) + " seconds" if timeout_s else ""}')
        execution_result = subprocess.run(command, capture_output=True, timeout=timeout_s)

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
        logging.warning(f'File {certificate_file!r} was not found')
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
    parser.add_argument(
        '-t', '--trusted-timestamping',
        action='store_true',
        help='Trusted timestamping')
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
        trusted_timestamping=args.trusted_timestamping,
        sign_timeout_s=args.sign_timeout)

    if result:
        print(f'File {in_file_path!r} was successfully signed.')
    else:
        sys.exit(f'FATAL ERROR: {in_file_path!r} was not signed.')


if __name__ == '__main__':
    main()
