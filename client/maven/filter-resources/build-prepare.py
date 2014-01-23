import os, sys, posixpath, shutil
sys.path.insert(0, '${basedir}/../common')
#sys.path.insert(0, os.path.join('..', '..', 'common'))

from gencomp import gencomp_cpp
#os.makedirs('${project.build.directory}/build/release/generated/')
gencomp_cpp(open('${project.build.sourceDirectory}/compatibility_info.cpp', 'w'))

help_source_dir = os.path.join('${environment.dir}', 'help', '${release.version}', '${customization}')
vox_source_dir = help_source_dir = os.path.join('${environment.dir}', 'festival.vox', 'lib')

if '${platform}' == 'windows':
    for arch in ('x86', 'x64'):
        help_dir = os.path.join('${libdir}', arch, 'bin/help')
        if os.path.exists(help_dir):
            shutil.rmtree(help_dir)
        shutil.copytree(help_source_dir, help_dir)
        for config in ('debug', 'release'):
            vox_dir = os.path.join('${libdir}', arch, 'bin', config, 'vox')
            if os.path.exists(vox_dir):
                shutil.rmtree(vox_dir)
            shutil.copytree(vox_source_dir, vox_dir)
else:     
    help_dir = os.path.join('${libdir}', 'bin/help')
    if os.path.exists(help_dir):
        shutil.rmtree(help_dir)
    shutil.copytree(help_source_dir, help_dir)            
    for config in ('debug', 'release'):
        vox_dir = os.path.join('${libdir}', 'bin', config, 'vox')
        if os.path.exists(vox_dir):
            shutil.rmtree(vox_dir)
        shutil.copytree(vox_source_dir, vox_dir)        
    