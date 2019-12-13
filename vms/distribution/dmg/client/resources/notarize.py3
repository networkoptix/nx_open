import os
import io
import sys
import time
import argparse
import plistlib
import datetime
import logging
import subprocess


def get_request_uuid(request_result):
    uuid_data = request_result.get("notarization-upload", None)
    return uuid_data["RequestUUID"] if uuid_data else None


def get_upload_error_message(request_result):
    error_data = request_result.get('product-errors')
    if not error_data or not len(error_data):
        return None

    return error_data[0].get('message')


def execute(command):
    try:
        output = subprocess.check_output(command, stderr=subprocess.DEVNULL)
        return True, output
    except subprocess.CalledProcessError as error:
        return False, error.output


def upload_for_notarization(options):
    command = [
        'xcrun', 'altool', '--notarize-app',
        '--output-format', 'xml',
        '-f', options.dmg_file_name,
        '--primary-bundle-id', options.bundle_id,
        '-itc_provider', options.teamId,
        '-u', options.user,
        '-p', options.password
    ]

    success, output = execute(command)

    request_result = plistlib.load(io.BytesIO(output))
    if not request_result:
        return None, None

    request_id = get_request_uuid(request_result)
    if success and request_id:
        return request_id, ""

    return None, get_upload_error_message(request_result)


def check_notarization_completion(options):
    command = [
        'xcrun', 'altool',
        '--notarization-info', options.request_id,
        '-u', options.user,
        '-p', options.password,
        '--output-format', 'xml'
    ]

    success, output = execute(command)
    progress_data = plistlib.load(io.BytesIO(output))
    info = progress_data.get('notarization-info')
    if not success or not info:
        return False, False, "Unexpected error: no data returned"

    status = info.get('Status', None)
    if not status:
        return False, False, "Enexpected error: no status field"

    if status == 'in progress':
        return False, False, "In progress"

    if status == 'success':
        return True, True, "Success"

    return True, False, "See errors in:\n{}".format(info.get('LogFileURL'))


def wait_for_notarization_completion(options, check_period_seconds):
    timeout = options.timeout * 60
    start_time = time.monotonic()
    completed = False
    while (time.monotonic() - start_time) < timeout and not completed:
        completed, success, error = check_notarization_completion(options)
        if not completed:
            time.sleep(check_period_seconds)
            continue

        return success, error

    return False, "Operation timeout"


def staple_app(dmg_file_name):
    command = ['xcrun', 'stapler', 'staple', dmg_file_name]
    success, output = execute(command)
    logging.debug(output)
    return success


def show_critical_error_and_exit(message):
    logging.critical(message)
    sys.exit(1)


def add_standard_parser_parameters(parser):
    parser.add_argument(
        '--user',
        metavar='<Apple Developer ID>',
        dest='user',
        required=True)

    parser.add_argument(
        '--password',
        metavar="<Apple Developer Password>",
        dest='password',
        required=True)

    parser.add_argument(
        '--timeout',
        metavar="<Timeout of operation in minutes>",
        dest='timeout',
        type=int,
        default=20)


def setup_notarize_parser(subparsers):
    notarizationParser = subparsers.add_parser('notarize')
    add_standard_parser_parameters(notarizationParser)
    notarizationParser.add_argument(
        '--team-id',
        metavar="<Apple Team ID>",
        dest='teamId',
        required=True)

    notarizationParser.add_argument(
        '--file-name',
        metavar="<Dmg File Name>",
        dest='dmg_file_name',
        required=True)

    notarizationParser.add_argument(
        '--bundle-id',
        metavar="<Application Bundle ID>",
        dest='bundle_id',
        required=True)


def setup_check_parser(subparsers):
    checkParsers = subparsers.add_parser('check')
    add_standard_parser_parameters(checkParsers)
    checkParsers.add_argument(
        '--request-id',
        metavar="<Request ID>",
        dest='request_id',
        required=True)


def main():
    logging.basicConfig(
        level=logging.INFO, format='%(levelname)s: %(message)s')

    parser = argparse.ArgumentParser(
        description="Notarizes specified application")
    subparsers = parser.add_subparsers()
    setup_notarize_parser(subparsers)
    setup_check_parser(subparsers)

    if not len(sys.argv) > 1:
        parser.print_help()
        exit()

    start_time = time.monotonic()
    options = parser.parse_args()
    need_upload = 'request_id' not in options or options.request_id is None
    if need_upload:
        options.request_id, upload_error = upload_for_notarization(options)

    if not options.request_id:
        message = "Can't upload package for notarization, see errors:\n {}\n"
        show_critical_error_and_exit(message.format(upload_error))

    if need_upload:
        logging.info("Upload successeful, upload id: %s", options.request_id)

    check_period_seconds = 30
    success, notarizationError = wait_for_notarization_completion(
        options, check_period_seconds)

    time_elapsed = time.monotonic() - start_time
    elapsedTimeMessage = "Total time: {}\n".format(
        datetime.timedelta(seconds=time_elapsed))
    if not success:
        message = "Notarization failed:\n{0}\n\n{1}\n"
        show_critical_error_and_exit(
            message.format(notarizationError, elapsedTimeMessage))

    if not staple_app(options.dmg_file_name):
        message = "Can't staple application!\n\n{1}"
        show_critical_error_and_exit(
            message.format(notarizationError, elapsedTimeMessage))

    logging.info("Notarization successeful!\n\n%s", elapsedTimeMessage)


if __name__ == '__main__':
    main()
