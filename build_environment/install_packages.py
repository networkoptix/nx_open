import os, sys
import platform
from pkg_resources import load_entry_point

pure_packages = [
    ('Django', '1.4.3_noptix'),
    ('South', '0.7.6'),
    ('certifi', '0.0.8'),
    ('chardet', '1.0.1'),
    ('django_model_utils', '1.0.0'),
    ('protobuf', '2.4.1'),
    ('requests', '0.11.1'),
    ('tornado', '2.4.1_noptix'),
    ('django_piston', '0.2.2_noptix'),
]

platform_packages = [
    ('netifaces', '0.7'),
]

additional_packages = [
    ('windows', 'x64', 'pywin32-218.win-amd64-py2.7.exe'),
    ('windows', 'x86', 'pywin32-218.win32-py2.7.exe'),

    ('windows', 'x64', 'cx_Logging-2.1-py2.7-win-amd64.egg'),
    ('windows', 'x86', 'cx_Logging-2.1-py2.7-win32.egg'),

    ('windows', 'x64', 'egenix_pyopenssl-0.13.0_1.0.1c_1-py2.7-win-amd64.egg'),
    ('windows', 'x86', 'egenix_pyopenssl-0.13.0_1.0.1c_1-py2.7-win32.egg'),
    ('linux', 'x64', 'egenix_pyopenssl-0.13.0_1.0.1c_1-py2.7-linux-x86_64.egg'),
    ('linux', 'x86', 'egenix_pyopenssl-0.13.0_1.0.1c_1-py2.7-linux-i686.egg'),

    ('windows', 'x64', 'cx_Freeze-4.3.1-py2.7-win-amd64.egg'),
    ('windows', 'x86', 'cx_Freeze-4.3.1-py2.7-win32.egg'),
    ('linux', 'x64', 'cx_Freeze-4.2.3-py2.7-linux-x86_64.egg'),
    ('linux', 'x86', 'cx_Freeze-4.2.3-py2.7-linux-i686.egg'),

    ('macosx', 'x64', 'pyOpenSSL-0.13-py2.7-macosx-10.8-intel.egg'),
]

def os_name():
    if sys.platform == 'win32':
        return 'windows'
    elif sys.platform == 'linux2':
        return 'linux'
    elif sys.platform == 'darwin':
        return 'macosx'

os_name = os_name()
is64 = platform.architecture()[0] == '64bit'
arch = is64 and 'x64' or 'x86'

def egg_platform_dir():
    return os.path.join('repo', os_name + '-' + arch)

def egg_pure_dir():
    return os.path.join('repo', 'noarch')

def platform_suffix(os_name, is64):
    if os_name == 'linux':
        return is64 and '-linux-x86_64' or '-linux-i686'
    elif os_name == 'windows':
        return is64 and '-win-amd64' or '-win32'
    elif os_name == 'macosx':
        return '-macosx-10.8-intel'

def egg_filename(package_name, version, platform_suffix = ''):
    return '%s-%s-py2.7%s.egg' % (package_name, version, platform_suffix)

easy_install = load_entry_point('setuptools', 'console_scripts', 'easy_install')
#def easy_install(x):
#    if not os.path.exists(x[1]):
#        print (x[1])

eggs = []

pure_repo_dir = egg_pure_dir()
for name, version in pure_packages:
    file_name = egg_filename(name, version)
    eggs.append(os.path.join(pure_repo_dir, file_name))

platform_repo_dir = egg_platform_dir()
for name, version in platform_packages:
    file_name = egg_filename(name, version, platform_suffix(os_name, is64))
    eggs.append(os.path.join(platform_repo_dir, file_name))

for platform, xarch, path in additional_packages:
    if platform == os_name and xarch == arch:
        eggs.append(os.path.join(platform_repo_dir, path))

llutil_egg_name = egg_filename('llutil', '0.2', platform_suffix(os_name, is64))
llutil_egg_path = os.path.join('packages', 'llutil', 'dist', llutil_egg_name)
if os.path.exists(llutil_egg_path):
    eggs.append(llutil_egg_path)
else:
    print os.path.join(platform_repo_dir, llutil_egg_name)
    eggs.append(os.path.join(platform_repo_dir, llutil_egg_name))

for egg_path in eggs:
    easy_install(['-ZN', egg_path])
