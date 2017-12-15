import fnmatch
import subprocess
import shutil
import os
import zipfile


def print_command(command):
    print '>> {0}'.format(subprocess.list2cmdline(command))


def execute_command(command, verbose=False):
    if verbose:
        print_command(command)
    try:
        subprocess.check_output(command, stderr=subprocess.STDOUT)
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


def find_files_by_template(dir, template):
    for file in fnmatch.filter(os.listdir(dir), template):
        yield os.path.join(dir, file)


def find_all_files(dir):
    for root, dirs, files in os.walk(dir):
        for file in files:
            yield os.path.join(root, file)


def zip_files(files, from_directory, zip_file, to_directory='.', mandatory=True):
    with zipfile.ZipFile(zip_file, "w", zipfile.ZIP_DEFLATED) as zip:
        for filename in files:
            source_file = os.path.join(from_directory, filename)
            if mandatory or os.path.isfile(source_file):
                zip.write(source_file, os.path.join(to_directory, filename))


def zip_files_to(zip, files, rel_path, target_path='.'):
    for file in files:
        target_filename = os.path.join(target_path, os.path.relpath(file, rel_path))
        zip.write(file, target_filename)
