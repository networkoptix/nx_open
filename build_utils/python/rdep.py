#!/usr/bin/env python

from __future__ import print_function
import argparse
import os
import sys
import shutil
import subprocess
import tempfile
import posixpath
try:
    import configparser
except ImportError:
    import ConfigParser as configparser

from rdep_config import RdepConfig, RepositoryConfig, PackageConfig

OS_IS_WINDOWS = sys.platform.startswith("win32") or sys.platform.startswith("cygwin")

TIMESTAMPS_FILE = "timestamps.dat"

ADDITIONAL_SYNC_ARGS = []
ADDITIONAL_UPLOAD_ARGS = []
if OS_IS_WINDOWS:
    ADDITIONAL_SYNC_ARGS = ["--chmod=ugo=rwx"]
    ADDITIONAL_UPLOAD_ARGS = ["--chmod=ugo=rwX"]


# Workaround against rsync bug: all paths with colon are counted as remote, so
# 'rsync rsync://server/path c:\test\path' won't work on windows.
def _cygwin_path(path):
    if len(path) > 1 and path[1] == ':':
        drive_letter = path[0].lower()
        return "/cygdrive/{0}{1}".format(drive_letter, path[2:].replace("\\", "/"))
    return path


def _is_remote_url(url):
    return ':' in url


def _is_rsync_url(url):
    return url.startswith("rsync://")


def _is_ssh_url(url):
    return _is_remote_url(url) and not _is_rsync_url(url)


def _find_root(path, file_name):
    while not os.path.isfile(os.path.join(path, file_name)):
        nextpath = os.path.dirname(path)
        if path == nextpath:
            return None
        else:
            path = nextpath

    return path


def _check_output(command):
    try:
        return subprocess.check_output(command, encoding='UTF-8')
    except TypeError:
        return subprocess.check_output(command)


class Rdep:
    SYNC_NOT_FOUND = 0
    SYNC_FAILED = 1
    SYNC_SUCCESS = 2

    def init_repository(path, url):
        if not os.path.isdir(path):
            print >> sys.stderr, "Directory does not exists: {0}".format(path)
            return False

        if not url:
            print >> sys.stderr, "Url cannot be empty"
            return False

        try:
            config = RepositoryConfig(os.path.join(path, ROOT_CONFIG_NAME))
            config.set_url(url)
        except Exception:
            print >> sys.stderr, "Could not init repository at {0}".format(path)
            return False

        print("Initialized repository at {0}".format(path))
        print("The repository will use URL = {0}".format(url))

        return True

    def __init__(self, root):
        self._config = RdepConfig()
        self._repo_config = RepositoryConfig(root)

        self.root = root
        self.verbose = False
        self.targets = None
        self.force = False
        self.fast_check = False

    def _verbose_message(self, message):
        if self.verbose:
            print(message)

    def _verbose_rsync(self, command):
        self._verbose_message("Executing rsync:\n{0}".format(" ".join(command)))

    def detect_package_and_target(self, path):
        if not path.startswith(self.root):
            return None, None

        path, target = os.path.split(path)
        package = None

        while path != self.root and target:
            package = target
            path, target = os.path.split(path)

        if path == self.root:
            return target, package

        return None, None

    def _get_rsync_command(
            self,
            source,
            destination,
            show_progress=True,
            additional_args=[]):

        command = [
            self._config.get_rsync("rsync"),
            "--recursive",
            "--delete",
            "--links",
            "--times"
        ]

        if not OS_IS_WINDOWS:
            command.append("--perms")

        if show_progress:
            command.append("--progress")

        if _is_ssh_url(source) or _is_ssh_url(destination):
            ssh = self._repo_config.get_ssh()
            if not ssh:
                ssh = self._config.get_ssh()
            if ssh:
                command.append("-e")
                command.append(ssh)

        command += additional_args

        command.append(source)
        if destination:
            command.append(destination)

        return command

    def _fix_permissions(self, path):
        if not OS_IS_WINDOWS:
            return

        command = ["icacls", path, "/reset", "/t"]
        self._verbose_message("Fixing permissions with: {}".format(" ".join(command)))
        subprocess.call(command)

    def load_timestamps_for_fast_check(self):
        self._timestamps = {}
        try:
            config = configparser.ConfigParser()
            config.optionxform = str
            config.read(os.path.join(self.root, TIMESTAMPS_FILE))

            for package, timestamp in config.items('Timestamps'):
                try:
                    self._timestamps[package] = int(timestamp)
                except Exception:
                    pass
        except Exception:
            pass

    def _stored_timestamp(self, target, package):
        try:
            return self._timestamps[posixpath.join(target, package)]
        except Exception:
            return None

    def _try_sync(self, target, package):
        url = self._repo_config.get_url()
        src = posixpath.join(url, target, package)
        dst = os.path.join(self.root, target, package)

        if self.fast_check:
            ts = self._stored_timestamp(target, package)
            package_ts = PackageConfig(dst).get_timestamp()

            if package_ts is None and ts is None:
                self._verbose_message(
                    "Treat package {0}/{1} as not found due to fast check".format(target, package))
                return self.SYNC_NOT_FOUND

            if package_ts is not None and ts is not None and ts <= package_ts:
                self._verbose_message(
                    "Skipping package {0}/{1} due to fast check".format(target, package))
                return self.SYNC_SUCCESS

        self._verbose_message(
            "root {0}\nurl {1}\ntarget {2}\npackage {3}\nsrc {4}\ndst {5}".format(
                self.root, url, target, package, src, dst))

        config_file = tempfile.mktemp()
        command = self._get_rsync_command(
            posixpath.join(src, PackageConfig.FILE_NAME),
            _cygwin_path(config_file),
            show_progress=False,
            additional_args=ADDITIONAL_SYNC_ARGS)
        self._verbose_rsync(command)
        with open(os.devnull, "w") as fnull:
            if subprocess.call(command, stderr=fnull) != 0:
                return self.SYNC_NOT_FOUND

        self._fix_permissions(config_file)

        newtime = PackageConfig(config_file).get_timestamp()
        if newtime is None:
            os.remove(config_file)
            return self.SYNC_NOT_FOUND

        time = PackageConfig(dst).get_timestamp()
        if newtime == time and not self.force:
            os.remove(config_file)
            return self.SYNC_SUCCESS

        if not os.path.isdir(dst):
            os.makedirs(dst)

        command = self._get_rsync_command(
            src + "/",
            _cygwin_path(dst),
            additional_args=ADDITIONAL_SYNC_ARGS + ["--exclude", PackageConfig.FILE_NAME]
        )
        self._verbose_rsync(command)

        if subprocess.call(command) != 0:
            os.remove(config_file)
            return self.SYNC_FAILED

        self._fix_permissions(dst)

        dst_config_file = os.path.join(dst, PackageConfig.FILE_NAME)
        self._verbose_message("Moving {0} to {1}".format(
            config_file, dst_config_file))
        shutil.copy(config_file, dst_config_file)
        os.remove(config_file)

        return self.SYNC_SUCCESS

    def sync_package(self, package):
        print("Synching {0}...".format(package))

        to_remove = []

        for target in self.targets:
            ret = self._try_sync(target, package)
            if ret == self.SYNC_NOT_FOUND:
                path = os.path.join(self.root, target, package)
                if os.path.isdir(path):
                    to_remove.append(path)
            else:
                break

        if ret == self.SYNC_FAILED:
            print >> sys.stderr, "Sync failed for {0}".format(package)
            return False
        elif ret == self.SYNC_NOT_FOUND:
            print >> sys.stderr, "Could not find {0}".format(package)
            return False

        for path in to_remove:
            print("Removing local {0}".format(path))
            shutil.rmtree(path)

        print("Package {0} downloaded for target {1}".format(package, target))
        return True

    def sync_packages(self, packages):
        if self.fast_check:
            self.load_timestamps_for_fast_check()

        for package in packages:
            if not self.sync_package(package):
                return False
        return True

    def upload_package(self, package):
        if len(self.targets) != 1:
            if len(self.targets) > 1:
                print >> sys.stderr, "Multiple targets for upload is not supported."
            else:
                print >> sys.stderr, "Please specify target for upload."
            return False

        target = self.targets[0]

        uploader_name = self._config.get_name()
        if not uploader_name:
            print >> sys.stderr, "Please specify your name in {0}".format(self._config.get_file_name())
            print >> sys.stderr, "Add \"name = Nx User <nxuser@networkoptix.com>\" to the section [General]."
            return False

        print("Uploading {0}...".format(package))

        url = self._repo_config.get_url()
        remote = posixpath.join(url, target, package)
        local = os.path.join(self.root, target, package)

        config_file = tempfile.mktemp()
        command = self._get_rsync_command(
            posixpath.join(remote, PackageConfig.FILE_NAME),
            _cygwin_path(config_file),
            show_progress=False,
            additional_args=ADDITIONAL_UPLOAD_ARGS)

        self._verbose_rsync(command)
        with open(os.devnull, "w") as fnull:
            if subprocess.call(command, stderr=fnull) == 0:
                self._fix_permissions(config_file)

                newtime = PackageConfig(config_file).get_timestamp()
                os.remove(config_file)
                time = PackageConfig(local).get_timestamp()
                if newtime and time != newtime:
                    print >> sys.stderr, "Somebody has already updated this package."
                    if not self.force:
                        print >> sys.stderr, "Please make sure you are updating the latest version of the package."
                        return False

        url = self._repo_config.get_push_url(url)
        remote = posixpath.join(url, target, package)

        config = PackageConfig(local)
        config.update_timestamp()
        config.set_uploader(uploader_name)

        command = self._get_rsync_command(
            _cygwin_path(local) + "/",
            remote,
            additional_args=ADDITIONAL_UPLOAD_ARGS + ["--exclude", PackageConfig.FILE_NAME]
        )

        self._verbose_rsync(command)

        if subprocess.call(command) != 0:
            print >> sys.stderr, "Could not upload {0}".format(package)
            return False

        command = self._get_rsync_command(
            _cygwin_path(os.path.join(local, PackageConfig.FILE_NAME)),
            remote,
            show_progress=False,
            additional_args=ADDITIONAL_UPLOAD_ARGS
        )

        self._verbose_rsync(command)

        if subprocess.call(command) != 0:
            print("Could not upload {0}".format(package))
            return False

        print("Done {0}".format(package))
        return True

    def upload_packages(self, packages):
        for package in packages:
            if not self.upload_package(package):
                return False
        return True

    def locate_package(self, package):
        package_config_path = (lambda path: os.path.join(path, PackageConfig.FILE_NAME))

        for target in self.targets:
            path = os.path.join(self.root, target, package)
            if os.path.exists(package_config_path(path)):
                return path

        return None

    def list_packages(self):
        if not self.targets:
            print >> sys.stderr, "Please specify target"
        elif len(self.targets) > 1:
            print >> sys.stderr, "Please specify only one target to list"

        target = self.targets[0]

        url = self._repo_config.get_url()
        url = posixpath.join(url, target) + "/"

        command = [self._config.get_rsync("rsync"), "--list-only", url]
        self._verbose_rsync(command)
        try:
            output = _check_output(command)
        except Exception:
            print("Could not list packages for {0}".format(target))
            return False

        for line in output.split('\n'):
            pos = line.rfind(' ')
            if pos >= 0:
                line = line[pos + 1:]

            if line and line != ".":
                print(line)

        return True

    def print_path(self, package):
        path = self.locate_package(package)
        if not path:
            print >> sys.stderr, "Package {0} not found.".format(package)
        print(path)

    def sync_timestamps(self):
        url = self._repo_config.get_url()
        url = posixpath.join(url, TIMESTAMPS_FILE)

        command = [self._config.get_rsync("rsync"), url, _cygwin_path(self.root)]
        command += ADDITIONAL_SYNC_ARGS
        self._verbose_rsync(command)
        try:
            subprocess.check_output(command)
            self._fix_permissions(os.path.join(self.root, TIMESTAMPS_FILE))
        except Exception:
            print("Could not sync timestamps file.")
            return False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root",               help="Repository root.")
    parser.add_argument("-f", "--force",        help="Force sync.",                         action="store_true")
    parser.add_argument("-u", "--upload",       help="Upload package to the repository.",   action="store_true")
    parser.add_argument("-v", "--verbose",      help="Additional debug output.",            action="store_true")
    parser.add_argument("-l", "--list",         help="List packages for target.",           action="store_true")
    parser.add_argument("-t", "--target",       help="Target.")
    parser.add_argument("--print-path",         help="Print package dir and exit.",         action="store_true")
    parser.add_argument("--init", metavar="URL", help="Init repository in the current dir with the specified URL.")
    parser.add_argument("--sync-timestamps", help="Sync timestamps file.", action="store_true")
    parser.add_argument(
        "--fast-check",
        action="store_true",
        help="Use timestamps file to check if package is outdated.")

    parser.add_argument("packages", nargs='*',  help="Packages to sync.")

    args = parser.parse_args()

    if args.init:
        return Rdep.init_repository(os.getcwd(), args.init)

    root = args.root
    if not root:
        root = _find_root(os.getcwd(), RepositoryConfig.FILE_NAME)
    if not root:
        print >> sys.stderr, "Repository root not found. Please specify it with --root or by setting working directory."
        return False

    rdep = Rdep(root)
    rdep.verbose = args.verbose
    rdep.force = args.force
    rdep.fast_check = args.fast_check

    if args.sync_timestamps:
        return rdep.sync_timestamps()

    if args.target:
        rdep.targets = [args.target]

    packages = args.packages
    if root and (not rdep.targets or not packages):
        detected_target, package = rdep.detect_package_and_target(os.getcwd())
        if detected_target and not rdep.targets:
            rdep.targets = [detected_target]
        if package and not packages:
            packages = [package]

    if not rdep.targets:
        print >> sys.stderr, "Please specify target."
        return False

    if args.list:
        return rdep.list_packages()

    if not packages:
        print >> sys.stderr, "No packages specified"
        return False

    if args.print_path:
        return rdep.print_path(packages[0])
    elif args.upload:
        return rdep.upload_packages(packages)
    else:
        return rdep.sync_packages(packages)


if __name__ == "__main__":
    if not main():
        exit(1)
