#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import argparse
import logging
import os
import subprocess
import time
from pathlib import Path


def run_and_log_command(command, **kwargs):
    logging.debug(f"Running {command}")
    return subprocess.run(command, **kwargs)


def prepare_keychain(keychain_name, keychain_password):
    keychain = \
        keychain_name if keychain_name.endswith(".keychain") else keychain_name + ".keychain"
    keychain_file = Path.home() / "Library" / "Keychains" / keychain

    keychains = subprocess.check_output(
        ["security", "list-keychains", "-d", "user"], encoding="utf-8").splitlines()
    keychains = [s.strip()[1:-1] for s in keychains]  # Strip whitespace and remove quotes.

    if str(keychain_file) not in keychains and f"{keychain_file}-db" not in keychains:
        logging.info(f"Creating keychain {keychain}")
        run_and_log_command(
            ["security", "create-keychain", "-p", keychain_password, keychain],
            stderr=None)
        run_and_log_command(["security", "set-keychain-settings", keychain])
        run_and_log_command(
            ["security", "list-keychains", "-d", "user", "-s", keychain, *keychains])

    logging.info(f"Unlocking keychain {keychain}")
    run_and_log_command(["security", "unlock-keychain", "-p", keychain_password, keychain])

    return keychain


def import_certificate(certificate_file, certificate_password, keychain=None, ignore_errors=False):
    certificate = Path(certificate_file)
    if not certificate.exists():
        return

    logging.info(f"Importing keys from {certificate}")
    if certificate_password is None:
        certificate_password = ""
    certificate_password_args = \
        ["-P", certificate_password] if certificate_file.endswith(".p12") else []
    keychain_args = ["-k", keychain] if keychain else []
    process = run_and_log_command(
        ["security",
            "import",
            certificate,
            "-T", "/usr/bin/codesign",
            *keychain_args,
            *certificate_password_args])

    return ignore_errors or process.returncode == 0


def set_key_partition_list(keychain_name, keychain_password):
    keychain = \
        keychain_name if keychain_name.endswith(".keychain") else keychain_name + ".keychain"

    run_and_log_command(
        ["security", "set-key-partition-list", "-S", "apple-tool:,apple:", "-k", keychain_password, keychain],
        stderr=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL)


def run_and_retry_if_needed(command, attempts=5):
    logging.debug(f"Run {attempts} attempts of command: {command}")
    for attempt in range(attempts):
        logging.debug(f"Attempt {attempt} of {attempts}")
        process = run_and_log_command(command)
        if process.returncode == 0:
            return True
        time.sleep(0.5)

    logging.error(f"Command failed after {attempts} attempts: {command}")


def sign_bundle(bundle_path, identity, entitlements=None, keychain=None):
    command = ["codesign", "-f", "-v", "--options", "runtime", "-s", identity]
    if entitlements:
        command += ["--entitlements", entitlements]
    if keychain:
        command += ["--keychain", keychain]

    logging.info("Signing QML plugins...")
    # codesign tool does not sign libraries and executables in 'Resources' folder when using --deep
    # option (known codesign issue). But if we move 'qml' folder to the appropriate folder (like
    # 'MacOS' or 'Frameworks') we need to create simlinks for all folders containing dots in their
    # path. It causes crash of desktop client when signed. To avoid this situation we move 'qml'
    # folder to 'Resources' and sign each library one by one.
    qml_dir = Path(bundle_path) / "Contents" / "Resources" / "qml"
    for lib in qml_dir.rglob("*.dylib"):
        logging.debug(f"Signing {lib}")
        if not run_and_retry_if_needed([*command, lib]):
            return

    # IMPORTANT Note: The main binary must be signed last. Otherwise the notarization of the bundle
    # will fail for unknown reason. It'll report that the main binary has an invalid signature
    # despite the fact it was successfully signed and `codesign` verification can prove that.
    logging.info("Signing the binary and libraries...")
    logging.debug(f"Signing {bundle_path}")
    if not run_and_retry_if_needed([*command, "--deep", bundle_path]):
        return

    return True


def command_import(args):
    keychain = prepare_keychain(args.keychain, args.keychain_password) if args.keychain else None
    if not import_certificate(
            args.certificate,
            args.certificate_password,
            keychain=keychain,
            ignore_errors=args.ignore_errors):
        exit(1)
    set_key_partition_list(args.keychain, args.keychain_password)


def command_sign(args):
    if not sign_bundle(
            args.bundle_path,
            args.identity,
            entitlements=args.entitlements,
            keychain=args.keychain):
        exit(1)


def setup_arguments_parser():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(required=True)

    parser_import = subparsers.add_parser(
        "import", help="Import keys from a p12 file into a keychain")
    parser_import.set_defaults(func=command_import)
    parser_import.add_argument("--certificate-password", "-P", help="Certificate file password")
    parser_import.add_argument("--keychain", "-k", help="Keychain name")
    parser_import.add_argument("--keychain-password", "-p", help="Keychain password")
    parser_import.add_argument("--ignore-errors", "-q", action="store_true", default=False,
        help="Ignore any errors when importing keys")
    parser_import.add_argument("certificate", help="Certificate p12 file to import")

    parser_sign = subparsers.add_parser("sign", help="Sign the macOS/iOS application")
    parser_sign.set_defaults(func=command_sign)
    parser_sign.add_argument("--identity", required=True, help="Developer identity")
    parser_sign.add_argument("--entitlements", help="Entitlements file")
    parser_sign.add_argument("--keychain", help="Keychain to look for the certificates in")
    parser_sign.add_argument("bundle_path", type=Path, help="Path to a bundle to sign")

    return parser


def main():
    logging.basicConfig(level=os.getenv("SIGNTOOL_LOG_LEVEL", "INFO"), format='%(message)s')
    parser = setup_arguments_parser()
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
