#!/usr/bin/env python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

"""Sign every file matching a pattern in a set of directories and WiX object files.

For a WiX object file (.wixobj), the files light packs from it are taken: running right before
light, this signs exactly the installer payload and nothing else that may be being built
concurrently.

Reuses the per-file sign command produced by nx_get_windows_sign_command (CMake). Everything after
the `--` separator is that base command; it must end with `--file`, so each file is signed with

    <base command> <file> --output <temp file>

With --skip-signed, files with a valid signature (checked by `signtool verify`) are skipped, so
untrusted or broken signatures get re-signed; with --require-timestamp, so do signatures without a
timestamp. Without signtool, only signature presence is checked, and --require-timestamp fails.
With --require-timestamp, a new signature without a timestamp is a failed attempt.

With --record-file, the signed files are recorded with --signer-id, their size and mtime. A
recorded file, unchanged since, is skipped without `signtool verify` (which also fails on an
untrusted test certificate) if it was signed with the same signer id, and with a timestamp if one
is required; otherwise (e.g. a Conan package file signed by a build of another customization in the
same build directory) it is re-signed.

A file signed for the first time keeps its mtime, so the tools copying a file only when its mtime
differs (e.g. Conan imports) don't overwrite it with the unsigned original. A re-signed file gets a
new mtime: light -reusecab compares only the size and mtime, and a new signature may keep the size.

A missing directory or file is an error: the installer would ship it unsigned or not at all.

Runs covering the same files or sharing the record file must not overlap; the build runs them one
at a time.
"""

import argparse
import concurrent.futures
import errno
import fnmatch
import json
import logging
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time
import xml.etree.ElementTree as ElementTree
from pathlib import Path
from typing import Optional

logger = logging.getLogger(__name__)

verify_timeout_s = 120
retry_delay_s = 5

# Installed on the build nodes by infra/deployment.
windows_sdk_bin_dir = (
    Path(os.environ.get('ProgramFiles(x86)', r'C:\Program Files (x86)')) / 'Windows Kits/10/bin')

# Windows error: the file cannot be moved to a different disk drive.
error_not_same_device = 17

# DER-encoded OIDs of the timestamp attributes of an Authenticode signature: the RFC 3161 one
# (1.3.6.1.4.1.311.3.3.1) and the legacy countersignature (1.2.840.113549.1.9.6).
timestamp_attribute_oids = (
    bytes.fromhex('060a2b060104018237030301'),
    bytes.fromhex('06092a864886f70d010906'),
)

# Column of the source file path in the WiX 3 object tables listing the packed files.
wixobj_source_columns = {'WixFile': 6, 'Binary': 1}


class FatalError(Exception):
    pass


class FileChanged(Exception):
    pass


def find_signtool() -> Optional[Path]:
    found = shutil.which('signtool')
    if found:
        return Path(found)

    candidates = []
    sdk_ver_bin_dir = os.environ.get('WindowsSdkVerBinPath')  # Set by the VS dev environment.
    if sdk_ver_bin_dir:
        candidates.append(Path(sdk_ver_bin_dir) / 'x64/signtool.exe')

    def version_key(directory: Path):
        try:
            return tuple(int(part) for part in directory.name.split('.'))
        except ValueError:
            return ()

    sdk_versions = sorted(windows_sdk_bin_dir.glob('10.*'), key=version_key, reverse=True)
    candidates += [directory / 'x64/signtool.exe' for directory in sdk_versions]

    return next((c for c in candidates if c.is_file()), None)


def has_valid_signature(signtool: Path, path: Path, require_timestamp: bool) -> bool:
    """True if signtool verifies the signature with the default Authenticode policy."""
    command = [str(signtool), 'verify', '/pa', '/q']
    if require_timestamp:
        command.append('/tw')  # A missing timestamp is a warning, exit code 2.
    try:
        result = subprocess.run(
            command + [str(path)], capture_output=True, timeout=verify_timeout_s)
    except subprocess.TimeoutExpired:
        # Re-signing would replace a valid signature, e.g. of Microsoft, with ours.
        raise FatalError(
            f'Signature verification took more than {verify_timeout_s} seconds: {path}')
    return result.returncode == 0


def certificate_table(path: Path):
    """The (offset, size) of the PE certificate table, or None. Reads only the headers."""
    # Up to the end of data directory entry 4 (security), the last of the PE32+ headers.
    pe_headers_size = 24 + 112 + 5 * 8
    try:
        with open(path, 'rb') as f:
            dos_header = f.read(0x40)
            if len(dos_header) < 0x40 or dos_header[:2] != b'MZ':
                return None
            e_lfanew = struct.unpack_from('<I', dos_header, 0x3C)[0]
            f.seek(e_lfanew)
            pe_headers = f.read(pe_headers_size)
    except OSError:
        return None

    if len(pe_headers) < 26 or pe_headers[:4] != b'PE\x00\x00':
        return None

    optional_header_offset = 24
    magic = struct.unpack_from('<H', pe_headers, optional_header_offset)[0]
    if magic == 0x10b:  # PE32
        data_directory_offset = optional_header_offset + 96
    elif magic == 0x20b:  # PE32+
        data_directory_offset = optional_header_offset + 112
    else:
        return None

    security_entry_offset = data_directory_offset + 4 * 8
    if security_entry_offset + 8 > len(pe_headers):
        return None

    return struct.unpack_from('<II', pe_headers, security_entry_offset)


def has_authenticode_signature(path: Path) -> bool:
    """True if the PE certificate table is non-empty. Reads only the headers."""
    table = certificate_table(path)
    return bool(table and table[1])


def has_timestamp(path: Path) -> bool:
    """True if the certificate table has a timestamp countersignature attribute.

    Unlike `signtool verify /tw`, this does not need a trusted certificate.
    """
    table = certificate_table(path)
    if not table or not table[1]:
        return False
    offset, size = table
    try:
        with open(path, 'rb') as f:
            f.seek(offset)
            certificates = f.read(size)
    except OSError:
        return False
    return any(oid in certificates for oid in timestamp_attribute_oids)


def is_complete_signed_file(path: Path) -> bool:
    """True if the file has a certificate table which ends within the file.

    The signature is appended to the end of the file, so a truncated download of a signed file
    lacks it or a part of it. Unlike `signtool verify`, this does not need a trusted certificate.
    """
    table = certificate_table(path)
    if not table or not table[1]:
        return False
    offset, size = table
    try:
        return offset + size <= path.stat().st_size
    except OSError:
        return False


def wixobj_packed_files(wixobj: Path):
    root = ElementTree.parse(wixobj).getroot()
    namespace = root.tag[:root.tag.index('}') + 1]
    for table in root.iter(f'{namespace}table'):
        column = wixobj_source_columns.get(table.get('name'))
        if column is None:
            continue
        for row in table.iter(f'{namespace}row'):
            fields = row.findall(f'{namespace}field')
            if column < len(fields) and fields[column].text:
                yield Path(fields[column].text)


def collect_files(directories, wixobj_files, patterns):
    """Return the files to sign and the missing directories and files."""
    candidates = []
    missing = []
    for directory in directories:
        root = Path(directory)
        if not root.is_dir():
            missing.append(root)
            continue
        for dirpath, _, filenames in os.walk(root):
            candidates += [Path(dirpath) / name for name in filenames]
    for wixobj in wixobj_files:
        candidates += wixobj_packed_files(wixobj)

    seen = set()
    files = []
    for path in candidates:
        if not any(fnmatch.fnmatch(path.name, p) for p in patterns):
            continue
        key = record_key(path)
        if key in seen:
            continue
        seen.add(key)
        if not path.is_file():
            missing.append(path)
            continue
        files.append(path)
    return files, missing


def file_state(path: Path):
    st = path.stat()
    return st.st_size, st.st_mtime_ns


def record_key(path: Path) -> str:
    return os.path.normcase(os.path.realpath(path))


def load_record(record_file: Optional[Path]) -> dict:
    if not record_file or not record_file.is_file():
        return {}
    try:
        return json.loads(record_file.read_text(encoding='utf-8'))
    except (OSError, ValueError) as e:
        logger.warning(f'Ignoring unreadable {record_file}: {e}')
        return {}


def save_record(record_file: Optional[Path], record: dict):
    if not record_file:
        return
    temp = record_file.with_name(record_file.name + '.tmp')
    temp.write_text(json.dumps(record, indent=1, sort_keys=True), encoding='utf-8')
    os.replace(temp, record_file)


def move_into_place(signed: Path, path: Path):
    """Replace path with signed atomically, leaving nothing next to path if possible."""
    try:
        os.replace(signed, path)
        return
    except OSError as e:
        if getattr(e, 'winerror', None) != error_not_same_device and e.errno != errno.EXDEV:
            raise
    # os.replace() is atomic only within one drive: copy next to the source first.
    staged = path.with_name(path.name + '.signing')
    try:
        shutil.copyfile(signed, staged)
        os.replace(staged, path)
    finally:
        staged.unlink(missing_ok=True)


def replace_with_signed(
        path: Path, signed: Path, state_before, keep_mtime: bool, retries: int,
        cancelled: threading.Event):
    """Replace path with signed. A retry does not sign the file again."""
    _, mtime_ns = state_before
    for attempt in range(1, retries + 1):
        if attempt > 1 and cancelled.wait(retry_delay_s * (attempt - 1)):
            raise OSError('cancelled')
        # Another build step (e.g. a linker) may have rewritten the file meanwhile.
        if file_state(path) != state_before:
            raise FileChanged()
        try:
            move_into_place(signed, path)
        except OSError as e:  # E.g. the file is opened by another process.
            if attempt == retries:
                raise
            logger.warning(f'Replace failed ({attempt}/{retries}) for {path}: {e}')
            continue
        if keep_mtime:
            try:
                os.utime(path, ns=(time.time_ns(), mtime_ns))
            except OSError as e:  # The file is signed: a retry would only sign it again.
                logger.warning(f'Signed, but the mtime is not restored for {path}: {e}')
        return


def sign_one(
        base_command, path: Path, signed: Path, retries: int, timeout_s: float,
        require_timestamp: bool, cancelled: threading.Event) -> Optional[bool]:
    """None if not signed, otherwise whether the signature has a timestamp."""
    # Sign into a separate temp file, so a failed attempt can't corrupt the source of the next one,
    # and nothing is left next to the source if the process is killed.
    command = base_command + [str(path), '--output', str(signed)]
    last_output = ''
    for attempt in range(1, retries + 1):
        if attempt > 1 and cancelled.wait(retry_delay_s * (attempt - 1)):
            break  # Another file failed: the run fails anyway.
        try:
            state_before = file_state(path)
            # Signing an unsigned file always changes its size, which light -reusecab notices.
            keep_mtime = not has_authenticode_signature(path)
            result = subprocess.run(command, capture_output=True, text=True, timeout=timeout_s)
            if result.returncode != 0:
                last_output = (result.stdout or '') + (result.stderr or '')
            elif not is_complete_signed_file(signed):
                last_output = 'the sign command produced an unsigned or truncated file'
            elif not (timestamped := has_timestamp(signed)) and require_timestamp:
                last_output = 'the sign command produced a signature without a timestamp'
            else:
                try:
                    replace_with_signed(
                        path, signed, state_before, keep_mtime, retries, cancelled)
                except FileChanged:
                    last_output = 'file changed while signing'
                except OSError as e:
                    logger.error(f'Giving up on {path}: signed, but not replaced: {e}')
                    return None
                else:
                    logger.info(f'Signed {path}')
                    return timestamped
        except subprocess.TimeoutExpired:
            last_output = f'no result in {timeout_s:.0f} seconds'
        except OSError as e:  # E.g. the file is locked by another process.
            last_output = str(e)
        finally:
            signed.unlink(missing_ok=True)
        logger.warning(f'Sign failed ({attempt}/{retries}) for {path}: {last_output.strip()}')
    logger.error(f'Giving up on {path}: {last_output.strip()}')
    return None


def main():
    argv = sys.argv[1:]
    if '--' not in argv:
        sys.exit("error: missing '--' separator before the base sign command")
    separator = argv.index('--')
    own_argv = argv[:separator]
    base_command = argv[separator + 1:]
    if not base_command:
        sys.exit('error: empty base sign command after --')

    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--directory', action='append', default=[], dest='directories',
        help='Directory to search recursively (repeatable).')
    parser.add_argument(
        '--wixobj', action='append', default=[], dest='wixobj_files', type=Path,
        help='WiX object file to take the files packed by light from (repeatable).')
    parser.add_argument(
        '--pattern', action='append', default=[], dest='patterns',
        help='Filename glob (repeatable, default: *.dll).')
    parser.add_argument(
        '--skip-signed', action='store_true',
        help='Skip files that already carry a valid Authenticode signature.')
    parser.add_argument(
        '--require-timestamp', action='store_true',
        help='With --skip-signed, re-sign files whose signature has no timestamp.')
    parser.add_argument('--jobs', type=int, default=4, help='Concurrent signing jobs.')
    parser.add_argument('--retries', type=int, default=3, help='Attempts per file.')
    parser.add_argument(
        '--sign-timeout', type=float, default=3600,
        help='Seconds to wait for one run of the sign command, its own retries included.')
    parser.add_argument(
        '--record-file', type=Path,
        help='Record of the signed files, to re-sign them when --signer-id changes.')
    parser.add_argument(
        '--signer-id', default='',
        help='Identifies the certificate the sign command uses, for --record-file.')
    parser.add_argument(
        '--temp-dir', type=Path,
        help='Directory for the temporary signed files (default: the system temp directory).')
    parser.add_argument(
        '--verbose', '-v', action='store_const', const=logging.DEBUG, default=logging.INFO,
        dest='log_level')
    args = parser.parse_args(own_argv)

    logging.basicConfig(level=args.log_level, format='%(levelname)-8s %(message)s')

    # Not tempfile.mkdtemp(): since Python 3.12.4, its directory is accessible only to the owner on
    # Windows, and a signed file moved out of it keeps that ACL instead of inheriting the one of
    # its new directory.
    temp_dir = (args.temp_dir or Path(tempfile.gettempdir())) / f'{os.getpid()}_{time.time_ns()}'
    temp_dir.mkdir(parents=True)
    try:
        record = load_record(args.record_file)
        try:
            sign_files(args, base_command, temp_dir, record)
        finally:
            save_record(args.record_file, record)
    except FatalError as e:
        sys.exit(f'FATAL ERROR: {e}')
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)


def recorded_signature(path: Path, record: dict, signer_id: str, require_timestamp: bool):
    """Whether the record says we signed the file as required now.

    True if we signed it, unchanged since, as required now; False if we signed it otherwise; None
    if the record knows nothing about the file as it is.
    """
    entry = record.get(record_key(path))
    if not entry:
        return None
    try:
        if (entry.get('size'), entry.get('mtime_ns')) != file_state(path):
            return None
    except OSError:
        return None
    return (entry.get('signer_id') == signer_id
        and (entry.get('timestamped', False) or not require_timestamp))


def sign_files(args, base_command, temp_dir: Path, record: dict):
    files, missing = collect_files(
        args.directories, args.wixobj_files, args.patterns or ['*.dll'])
    if missing:
        for path in missing:
            logger.error(f'Does not exist: {path}')
        sys.exit(f'FATAL ERROR: {len(missing)} directory(ies) or file(s) to sign do not exist.')
    if args.skip_signed and files:  # Without files, a missing signtool does not matter.
        signtool = find_signtool()
        if signtool:
            logger.info(f'Verifying existing signatures with {signtool}')

            def has_valid(path):
                # The cheap header check spares a signtool run for unsigned files.
                return (has_authenticode_signature(path)
                    and has_valid_signature(signtool, path, args.require_timestamp))
        elif args.require_timestamp:
            # A release would ship whatever signature it finds, without a timestamp too.
            raise FatalError(
                'signtool.exe was not found, existing signatures cannot be verified.')
        else:
            logger.warning(
                'signtool.exe was not found, existing signatures are not verified: a file with '
                'an untrusted or broken signature will not be re-signed.')
            has_valid = has_authenticode_signature

        def is_signed(path):
            recorded = recorded_signature(
                path, record, args.signer_id, args.require_timestamp)
            if recorded is None:
                return has_valid(path)
            if not recorded:
                logger.info(f'Re-signing, signed with another signer id or no timestamp: {path}')
            return recorded

        with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as executor:
            signed_flags = list(executor.map(is_signed, files))
        kept = [f for f, signed in zip(files, signed_flags) if not signed]
        skipped = len(files) - len(kept)
        if skipped:
            logger.info(f'Skipping {skipped} already-signed file(s).')
        files = kept

    if not files:
        logger.info('Nothing to sign.')
        return

    logger.info(f'Signing {len(files)} file(s) with {args.jobs} job(s).')
    failures = []
    cancelled = threading.Event()
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as executor:
        # The index keeps the temp names of same-named files from different directories apart; the
        # name keeps the extension, which the signing tools may rely on.
        future_to_path = {
            executor.submit(
                sign_one, base_command, path, temp_dir / f'{i}_{path.name}', args.retries,
                args.sign_timeout, args.require_timestamp, cancelled): path
            for i, path in enumerate(files)}
        for future in concurrent.futures.as_completed(future_to_path):
            if future.cancelled():
                continue
            path = future_to_path[future]
            timestamped = future.result()
            if timestamped is not None:
                try:
                    size, mtime_ns = file_state(path)
                except OSError as e:  # Removed right after signing: nothing to record.
                    logger.warning(f'Not recorded: {e}')
                    record.pop(record_key(path), None)
                    continue
                record[record_key(path)] = {
                    'signer_id': args.signer_id, 'timestamped': timestamped,
                    'size': size, 'mtime_ns': mtime_ns}
                continue
            failures.append(path)
            if not cancelled.is_set():
                # The run fails anyway: don't sign the rest of the files.
                cancelled.set()
                for pending in future_to_path:
                    pending.cancel()

    if failures:
        not_attempted = sum(1 for f in future_to_path if f.cancelled())
        sys.exit(
            f'FATAL ERROR: {len(failures)} file(s) were not signed, '
            f'{not_attempted} file(s) were not attempted.')
    logger.info('All files signed.')


if __name__ == '__main__':
    main()
