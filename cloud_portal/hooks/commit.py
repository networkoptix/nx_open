# Add this to .hgrc file
# [hooks]
# precommit = python:~/develop/nx_vms/cloud_portal/hooks/commit.py:filter_dev_keywords
#

import re
import subprocess
from os.path import expanduser


def filter_dev_keywords(ui, repo, **kwargs):
    home = expanduser("~") + "/develop/nx_vms/"
    keys = ["console.log", "debugger;", "fuck"]

    output = subprocess.check_output(["hg", "status", "-m"])
    matches_found = False

    files = [line.split(' ', 1)[1] for line in output.split('\n') if line != '']

    for f in files:
        content = open(home + f).read()
        for key in keys:
            if re.search(key, content, re.IGNORECASE):
                ui.warn("Found {" + key + "} in " + f + "\n")
                matches_found = True

    return matches_found  # Mercurial hook expects 0 for success and 1 for fail
