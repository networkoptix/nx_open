"""
This script is a conditional launcher for another python script. It checks whether the modification
time of the file passed via --checker-flag-file is older than the modification time of the
FETCH_HEAD flie of the git working tree directory passed via --checker-source-dir. If true, then
the script passed via --checker-run-script is executed, otherwise nothing happens. It is possible
to set some paths to PYTHONPATH for the script to run via --checker-pythonpath.

The script to run is loaded and run as a python module, therefore:
1. The name of the script to run must be passed as a file name without .py extension.
2. PYTHONPATH must contain the path to the script to run (e.g if you want to run a
file that resides in "/tmp", --checker-pythonpath must contain at least the "/tmp" value.
3. The script to run must have the "main" function as an entry point.
4. If the script to run uses the "argparse" module, it must not use parse_args(), and can use
parse_known_args() instead.
5. The script to run must not have parameters with the same names as the parameters of the
launcher script (this one). See the list of such parameters in this documentation.
"""

from pathlib import Path
import argparse
import sys
import re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--checker-flag-file",
        required=True,
        help="The file which timestamp should be checked against FETCH_HEAD timestamp")
    parser.add_argument("--checker-source-dir",
        required=True,
        help="Source tree directory")
    parser.add_argument("--checker-run-script",
        required=True,
        help="Script to run")
    parser.add_argument("--checker-pythonpath",
        nargs="*",
        default=[],
        required=True,
        help="Paths to add to PYTHONPATH")

    args, _ = parser.parse_known_args()

    if not is_fetch_timestamp_newer_than_timestamps_file(
        sources_dir=args.checker_source_dir, timestamp_file=args.checker_flag_file):
        exit(0)

    # Update the flag file timestamp to prevent the python script from running next time when
    # "run_after_fetch" is called if nothing is changed.
    Path(args.checker_flag_file).touch()

    sys.path += args.checker_pythonpath

    script = __import__(args.checker_run_script)
    script.main()


def is_fetch_timestamp_newer_than_timestamps_file(sources_dir, timestamp_file):
    git_root_path = Path(sources_dir).joinpath(".git")
    if git_root_path.is_file():
        with open(git_root_path) as f:
            git_root_path_file_content = f.readline().rstrip()
            _, _, real_git_root_path_string = git_root_path_file_content.partition(": ")
            real_git_root_path = Path(real_git_root_path_string)
    else:
        real_git_root_path = git_root_path

    if real_git_root_path.is_dir():
        fetch_head_path = real_git_root_path.joinpath("FETCH_HEAD")
        if not fetch_head_path.is_file():
            # FETCH_HEAD is not a file so no fetches have been done yet; no need to do packages
            # synchronization.
            return False

        fetch_head_timestamp_s = fetch_head_path.stat().st_mtime

    else:
        print(
            f"WARNING: No repository detected in {Path(sources_dir).as_posix()!r},"
            " trying to use git_info.txt.")

        git_info_path = Path(sources_dir).joinpath("git_info.txt")
        if not git_info_path.is_file():
            print(f"WARNING: {git_info_path.as_posix()!r} is not found. Forcing script run.")
            return True

        with open(git_info_path) as f:
            content = f.readlines()
            for line in content:
                match = re.match(r"^fetchTimestampS=(?P<fetch_timestamp>[\d\.,]+)", line)
                if match:
                    fetch_head_timestamp_s = float(match['fetch_timestamp'].replace(",", "."))
                    break
            else:
                print(
                    'WARNING: "fetchTimestampS" field is not found in'
                    f'{git_info_path.as_posix()!r}. Forcing script run.')
                return True

    timestamp_file_path = Path(timestamp_file)
    if not timestamp_file_path.is_file():
        return True  #< No packages synchronization has been done yet; have to do it now.

    return fetch_head_timestamp_s > timestamp_file_path.stat().st_mtime


if __name__ == "__main__":
    main()
