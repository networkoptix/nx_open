#!/usr/bin/env python

from __future__ import print_function
import os
import sys
import time
import distutils.spawn
import rdep_config

OS_IS_WINDOWS = sys.platform.startswith("win32") or sys.platform.startswith("cygwin")
REPOSITORY_PATH = os.getenv("RDEP_PACKAGES_DIR")
if not REPOSITORY_PATH:
    # TODO: This block is for backward compatibility. Remove it when CI is re-configured.
    REPOSITORY_PATH = os.getenv("environment")
    if REPOSITORY_PATH:
        REPOSITORY_PATH = os.path.join(REPOSITORY_PATH, "packages")
SYNC_URL = "rsync://mono.enk.me/buildenv/rdep/packages"
if time.timezone == 28800:
    SYNC_URL = "rsync://la.hdw.mx/buildenv/rdep/packages"
PUSH_URL = "rsync@mono.enk.me:buildenv/rdep/packages"

def configure(print_summary = False):
    if not os.path.isdir(REPOSITORY_PATH):
        os.makedirs(REPOSITORY_PATH)

    config = rdep_config.RepositoryConfig(REPOSITORY_PATH)
    if not config.get_url():
        config.set_url(SYNC_URL)
    if not config.get_push_url(None):
        config.set_push_url(PUSH_URL)

    global_config = rdep_config.RdepConfig()

    rsync = global_config.get_rsync()
    if not rsync:
        rsync = distutils.spawn.find_executable("rsync")
        if not rsync:
            if not OS_IS_WINDOWS:
                print(
                    "Cannot find rsync executable. Please install it or specify in .rderc",
                    file=sys.stderr)
                return False

            global_config.set_rsync(rsync)

    if not global_config.get_name():
        homedir = os.path.join(os.path.expanduser("~"))
        hg_config_file = os.path.join(homedir, ".hgrc")
        if not os.path.isfile(hg_config_file):
            hg_config_file = os.path.join(homedir, "Mercurial.ini")
        if os.path.isfile(hg_config_file):
            hg_config = rdep_config.ConfigHelper(hg_config_file)
            username = hg_config.get_value("ui", "username", "").strip()
            if username:
                global_config.set_name(username)

    if print_summary:
        print("Rdep repository is ready.")
        print("  Path =", REPOSITORY_PATH)
        print("  Sync URL =", SYNC_URL)
        print("  Push URL =", PUSH_URL)
        print("  Rsync =", rsync)

    return True

if __name__ == '__main__':
    if not configure(print_summary = True):
        exit(1)
