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

    target_dir = join('${project.build.directory}', arch, 'bin')
    bin_source_dir = '${qt.dir}/bin'
    plugin_source_dir = '${qt.dir}/plugins'

    if not os.path.exists(target_dir):
        mkdir_p(target_dir)        
    
    for qtlib in qtlibs:
        if qtlib != '':
            for file in os.listdir(bin_source_dir):
                if fnmatch.fnmatch(file, 'qt5%sd.dll' % qtlib):
                    shutil.copy2(join(bin_source_dir, file), join(target_dir, 'debug'))
                elif fnmatch.fnmatch(file, 'qt5%s.dll' % qtlib):
                    shutil.copy2(join(bin_source_dir, file), join(target_dir, 'release'))
    
    


    
    for qtplugin in qtplugins:
        for config in ('debug', 'release'):
            if not os.path.exists(join(target_dir, config, qtplugin)):
                os.makedirs (join(target_dir, config, qtplugin))    
        
        if qtplugin != '':
            print join(plugin_source_dir, qtplugin)
            for file in os.listdir(join(plugin_source_dir, qtplugin)):
                if fnmatch.fnmatch(file, 'q*d.dll'):
                    shutil.copy2(join(plugin_source_dir, qtplugin, file), join(target_dir, 'debug', qtplugin))
                else:
                    shutil.copy2(join(plugin_source_dir, qtplugin, file), join(target_dir, 'release', qtplugin))
            
            #distutils.dir_util.copy_tree(join(plugin_source_dir, qtplugin), join(target_dir, qtplugin))                        
    
    for config in ('debug', 'release'):
        distutils.dir_util.copy_tree('vox', join(target_dir, config, 'vox'))                        
        shutil.copy2('${root.dir}/quicksyncdecoder/hw_decoding_conf.xml', join(target_dir, config))
    #shutil.rmtree('festival-vox')
