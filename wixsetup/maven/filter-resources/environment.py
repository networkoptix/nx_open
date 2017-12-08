import subprocess
import shutil
import os

arch = '${arch}'
wix_directory = '${wix_directory}/bin'
qt_directory = '${qt.dir}'
build_configuration = '${build.configuration}'
installer_cultures = '${installer.cultures}'
installer_language = '${installer.language}'
distribution_output_dir = '${distribution_output_dir}'

# Source directories
bin_source_dir = '${bin_source_dir}'
vox_source_dir = '${VoxSourceDir}'
vcrt14_source_dir = '${VC14RedistPath}/bin'
client_qml_source_dir = '${ClientQmlDir}'
client_help_source_dir = '${ClientHelpSourceDir}'
client_fonts_source_dir = '${ClientFontsDir}'
client_background_source_dir = '${ClientBgSourceDir}'

# Signing section
signtool_directory = '${signtool_directory}/bin'
main_certificate = '${certificates_path}/wixsetup/${sign.cer}'
additional_certificate = '${sign.intermediate.cer}'
sign_password = '${sign.password}'
sign_description = '"${company.name} ${display.product.name}"'

# Distrib names
client_distribution_name = '${client_distribution_name}'
server_distribution_name = '${server_distribution_name}'
bundle_distribution_name = '${bundle_distribution_name}'
servertool_distribution_name = '${servertool_distribution_name}'
paxton_distribution_name = '${paxton_distribution_name}'

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
