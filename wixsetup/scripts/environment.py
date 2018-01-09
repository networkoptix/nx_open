import fnmatch
import subprocess
import shutil
import os
import zipfile


def print_command(command):
    print '>> {0}'.format(subprocess.list2cmdline(command))


def execute_command(command, verbose=False, working_directory=None):
    if verbose:
        print_command(command)
    try:
        current_directory = os.getcwd()
        if working_directory:
            os.chdir(working_directory)
        subprocess.check_output(command, stderr=subprocess.STDOUT)
        if working_directory:
            os.chdir(current_directory)
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


def ffmpeg_files(source_dir):
    templates = [
        'av*.dll',
        'libogg*.dll',
        'libvorbis*.dll',
        'libmp3*.dll',
        'swscale-*.dll',
        'swresample-*.dll']
    for template in templates:
        for file in find_files_by_template(source_dir, template):
            yield file


def openssl_files(source_dir):
    yield os.path.join(source_dir, 'libeay32.dll')
    yield os.path.join(source_dir, 'ssleay32.dll')


def openal_files(source_dir):
    yield os.path.join(source_dir, 'OpenAL32.dll')


def quazip_files_to(source_dir):
    yield os.path.join(source_dir, 'quazip.dll')


def icu_files(qt_bin_dir):
    for file in find_files_by_template(qt_bin_dir, 'icu*.dll'):
        yield file


def qt_files(qt_bin_dir, libs):
    for lib in libs:
        yield os.path.join(qt_bin_dir, 'Qt5{}.dll'.format(lib))


def qt_plugins_files(qt_plugins_dir, libs):
    for lib in libs:
        yield os.path.join(qt_plugins_dir, lib)


def nx_files(source_dir, libs):
    for lib in libs:
        yield os.path.join(source_dir, '{}.dll'.format(lib))


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


def zip_rdep_package_to(zip, package_directory):
    bin_directory = os.path.join(package_directory, 'bin')
    zip_files_to(zip, find_all_files(bin_directory), bin_directory)

