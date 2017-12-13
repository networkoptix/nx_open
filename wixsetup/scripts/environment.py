import subprocess
import shutil
import os

def print_command(command):
    print '>> {0}'.format(subprocess.list2cmdline(command))

def execute_command(command, verbose = False):
    if verbose:
        print_command(command)
    try:
        subprocess.check_output(command, stderr = subprocess.STDOUT)
    except Exception as e:
        if not verbose:
            print_command(command)
        print "Error: {0}".format(e.output)
        raise

def rename(folder, old_name, new_name):
    full_old_name = os.path.join(folder, old_name)
    full_new_name = os.path.join(folder, new_name)
    if os.path.exists(full_new_name):
        os.unlink(full_new_name)
    if os.path.exists(full_old_name):
        shutil.copy2(full_old_name, full_new_name)
