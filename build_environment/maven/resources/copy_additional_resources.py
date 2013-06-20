import io, sys, os, errno, platform, shutil, fnmatch, distutils.core
from os.path import dirname, join, exists, isfile
#from main import get_environment_variable, cd

qtlibs = ['${qtlib1}', '${qtlib2}', '${qtlib3}', '${qtlib4}', '${qtlib5}', '${qtlib6}', '${qtlib7}', '${qtlib8}']
qtplugins = ['${qtplugin1}', '${qtplugin2}', '${qtplugin3}']

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc: # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else: raise

for arch in ('x86', 'x64'):
    
    distutils.dir_util.copy_tree('help', join('${project.build.directory}', arch, 'bin/help'))                        
    #shutil.rmtree('help')
    
    for config in ('debug', 'release'):

        target_dir = join('${project.build.directory}', arch, 'bin', config)
        bin_source_dir = join('${environment.dir}/qt/bin', arch, config)
        plugin_source_dir = join('${environment.dir}/qt/plugins', arch, config)

        if not os.path.exists(target_dir):
            mkdir_p(target_dir)        
        
        for qtlib in qtlibs:
            if qtlib != '':
                for file in os.listdir(bin_source_dir):
                    if fnmatch.fnmatch(file, 'qt%s*.dll' % qtlib):
                        shutil.copy2(join(bin_source_dir, file), target_dir)
        
        shutil.copy2('${root.dir}/quicksyncdecoder/hw_decoding_conf.xml', target_dir)   
                        
        for qtplugin in qtplugins:
            if qtplugin != '':
                print join(plugin_source_dir, qtplugin)
                distutils.dir_util.copy_tree(join(plugin_source_dir, qtplugin), join(target_dir, qtplugin))                        
        
        distutils.dir_util.copy_tree('festival-vox', join(target_dir, 'festival-vox'))                        
        #shutil.rmtree('festival-vox')
