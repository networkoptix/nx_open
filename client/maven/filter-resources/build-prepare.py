import os, sys, posixpath, shutil
sys.path.insert(0, '${basedir}/../common')
#sys.path.insert(0, os.path.join('..', '..', 'common'))

from gencomp import gencomp_cpp
#os.makedirs('${project.build.directory}/build/release/generated/')
gencomp_cpp(open('${project.build.sourceDirectory}/compatibility_info.cpp', 'w'))

help_source_dir = '${ClientHelpSourceDir}'
vox_source_dir = '${ClientVoxSourceDir}'
target_dir = '${libdir}'

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
            target_plugins = os.path.join(target_dir, arch, 'bin', config, 'plugins')
            if os.path.exists(os.path.join(target_plugins, 'hw_decoding_conf.xml')):
                os.unlink(os.path.join(target_plugins, 'hw_decoding_conf.xml'))
            if '${quicksync}' == 'true':
                if not os.path.exists(target_plugins):
                    os.makedirs(target_plugins)
                shutil.copy2('${root.dir}/plugins/quicksyncdecoder/hw_decoding_conf.xml', target_plugins)
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
    