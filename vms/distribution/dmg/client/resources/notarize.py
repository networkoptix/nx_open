#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import argparse
import datetime
import json
import logging
import os
import subprocess
import sys
import time


def masked_command(command, masked_secrets=()):
    def masked_arg(arg):
        for secret in masked_secrets:
            arg = arg.replace(secret, "*****")
        return arg

    return [masked_arg(arg) for arg in command]


def execute(command, masked_secrets=()):
    try:
        logging.info(f"Running: {masked_command(command, masked_secrets)}")
        output = subprocess.check_output(command)
        return output
    except subprocess.CalledProcessError as error:
        logging.error(f"Command execution failed. Output: {output}")


def upload_for_notarization(args):
    command = [
        "xcrun", "notarytool", "submit",
        args.dmg_file_name,
        "--team-id", args.team_id,
        "--apple-id", args.apple_id,
        "--password", args.password,
        "--output-format", "json"
    ]

    output = execute(command, masked_secrets=[args.apple_id, args.password])
    if output is None:
        return False

    try:
        request_result = json.loads(output)
    except json.JSONDecodeError:
        logging.error(f"Failed to parse command output. Output: {output}")
        return False

    request_id = request_result.get("id")
    if not request_id:
        logging.error(f"Upload unsuccessful: {request_result.get('message')}")
        return False

    logging.info(f"Upload successful, upload id: {request_id}")
    return request_id


def check_notarization_status(request_id, args):
    command = [
        "xcrun", "notarytool",
        "info", request_id,
        "--apple-id", args.apple_id,
        "--password", args.password,
        "--team-id", args.team_id,
        "--output-format", "json"
    ]

    output = execute(command, masked_secrets=[args.apple_id, args.password])
    if output is None:
        return False

    try:
        info = json.loads(output)
    except json.JSONDecodeError:
        logging.error(f"Failed to parse command output. Output: {output}")
        return False

    status = info.get("status").lower()
    if not status:
        logging.error(f"There's no status in the command output: {output}")
        return False

    if status == "in progress" or status == "accepted":
        return status

    message = info.get("message")
    logging.error(f"Notarization failed. Status: {status}. Message: {message}.")

    return False


def wait_for_notarization_completion(request_id, args):
    timeout = args.timeout * 60
    start_time = time.monotonic()

    while (time.monotonic() - start_time) < timeout:
        status = check_notarization_status(request_id, args)

        if status == "in progress":
            retry_period = 30
            logging.info(f"Notarization is in progress. Waiting {retry_period} sec...")
            time.sleep(retry_period)
            continue

        if status == "accepted":
            return True

        return False


def staple_app(dmg_file_name):
    command = ["xcrun", "stapler", "staple", dmg_file_name]
    output = execute(command)
    if output is None:
        logging.error("Cannot staple application.")
        return False

    logging.debug(output)
    return True


def show_critical_error_and_exit(message):
    logging.critical(f"FAILED: {message}")
    sys.exit(1)


def main():
    logging.basicConfig(level=logging.DEBUG, format="%(message)s")

    parser = argparse.ArgumentParser(description="Notarizes the specified application")
    parser.add_argument("--apple-id",
        help="Apple ID can also be specified with NOTARIZATION_USER environment variable")
    parser.add_argument("--password",
        help="Password can also be specified with NOTARIZATION_PASSWORD environment variable")
    parser.add_argument("--team-id", help="Apple team ID", required=True)
    parser.add_argument("--timeout", help="Timeout of operation in minutes", type=int, default=20)
    parser.add_argument("--continue-with-request-id", dest="request_id",
        help="Continue a timed out notarization process using the specified request ID")
    parser.add_argument("dmg_file_name", help="DMG file name")

    args = parser.parse_args()

    start_time = time.monotonic()

    if not args.apple_id:
        args.apple_id = os.getenv("NOTARIZATION_USER")
        if not args.apple_id:
            show_critical_error_and_exit(
                "Notarization user is not specified. See --help for details.")
    if not args.password:
        args.password = os.getenv("NOTARIZATION_PASSWORD")
        if not args.password:
            show_critical_error_and_exit(
                "Notarization password is not specified. See --help for details.")

    if not os.path.exists(args.dmg_file_name):
        show_critical_error_and_exit(f"File {args.dmg_file_name} does not exist.")

    if not args.request_id:
        request_id = upload_for_notarization(args)
    else:
        request_id = args.request_id

    if not request_id:
        show_critical_error_and_exit(
            f"Cannot upload package for notarization, see errors: {error}")

    result = wait_for_notarization_completion(request_id, args)

    time_elapsed = time.monotonic() - start_time
    logging.info(f"Total time: {datetime.timedelta(seconds=time_elapsed)}")

    if not result:
        sys.exit(1)

    if not staple_app(args.dmg_file_name):
        sys.exit(1)

    logging.info("Notarization successful.")


if __name__ == "__main__":
    main()
