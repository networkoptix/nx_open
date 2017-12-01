import subprocess
import shutil

wix_directory = '${wix_directory}/bin'
qt_directory = '${qt.dir}'
build_configuration = '${build.configuration}'
installer_cultures = '${installer.cultures}'
installer_language = '${installer.language}'
bin_source_dir = '${bin_source_dir}'

def execute_command(command, verbose = False):
    if verbose:
        print '>> {0}'.format(' '.join(command))
    try:
        subprocess.check_output(command, stderr = subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        print "Error: {0}".format(e.output)
        raise

def rename(folder, old_name, new_name):
    if os.path.exists(join(folder, new_name)):
        os.unlink(join(folder, new_name))
    if os.path.exists(join(folder, old_name)):
        shutil.copy2(join(folder, old_name), join(folder, new_name))
