import io, sys, os, errno, platform, shutil, fnmatch, tarfile
from os.path import dirname, join, exists, isfile

basedir = join(dirname(os.path.abspath(__file__)))
sys.path.append(basedir + '/' + '../..')

from main import get_environment_variable, cd

plugin_source_dir = '${qt.dir}/plugins'
qtlibs = ['${qtlib1}', '${qtlib2}', '${qtlib3}', '${qtlib4}', '${qtlib5}', '${qtlib6}', '${qtlib7}', '${qtlib8}', '${qtlib9}', '${qtlib10}', '${qtlib11}', '${qtlib12}']
qtplugins = ['${qtplugin1}', '${qtplugin2}', '${qtplugin3}']

def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc: # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else: raise

print '+++++++++++++++++++++ EXPANDING LIBS +++++++++++++++++++++'
        
for file in os.listdir('.'):
    if file.endswith('tar.gz'):
        print(file)
        tar = tarfile.open(file)
        tar.extractall()
        tar.close() 
        os.unlink(file)

print '+++++++++++++++++++++ COPYING QT LIBS +++++++++++++++++++++'
        
if get_environment_variable('platform') == 'windows':        
    for arch in ('x86', 'x64'):

        lib_source_dir = '${qt.dir}/bin'
        target_dir = join('${project.build.directory}', arch, 'bin')


        help_dir = join('${project.build.directory}', arch, 'bin/help')
            
        if os.path.exists(help_dir):
            shutil.rmtree(help_dir)
            shutil.copytree('help', help_dir)                        

        #shutil.rmtree('help')

        if not os.path.exists(target_dir):
            mkdir_p(target_dir)        
        
        for qtlib in qtlibs:
            if qtlib != '':
                print(qtlib)
                for file in os.listdir(lib_source_dir):
                    if fnmatch.fnmatch(file, 'qt5%sd.dll' % qtlib):
                        shutil.copy2(join(lib_source_dir, file), join(target_dir, 'debug'))
                    elif fnmatch.fnmatch(file, 'qt5%s.dll' % qtlib):
                        shutil.copy2(join(lib_source_dir, file), join(target_dir, 'release'))    
        
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

else:     
    lib_source_dir = '${qt.dir}/lib'
    lib_target_dir = join('${project.build.directory}', 'lib')
    target_dir = join('${project.build.directory}', 'bin')
    help_dir = join('${project.build.directory}', 'bin/help')
    
    if os.path.exists(help_dir):
        shutil.rmtree(help_dir)
        shutil.copytree('help', help_dir)    
    
    for qtlib in qtlibs:
        if qtlib != '': 
            for file in os.listdir(lib_source_dir):            
                for config in ('debug', 'release'):
                    if fnmatch.fnmatch(file, 'libQt5%s.so.*' % qtlib):
                        print (join(lib_source_dir, file))
                        srcname = join(lib_source_dir, file)
                        dstname = join(lib_target_dir, config, file)
                        if os.path.islink(srcname):
                            linkto = os.readlink(srcname)
                            os.symlink(linkto, dstname)
                        else:
                            shutil.copy2(srcname, dstname)       
                              
    for config in ('debug', 'release'):
        shutil.copy2('${root.dir}/quicksyncdecoder/hw_decoding_conf.xml', join(target_dir, config))  

        for qtplugin in qtplugins:
            if qtplugin != '':
                print join(plugin_source_dir, qtplugin)
                shutil.copytree(join(plugin_source_dir, qtplugin), join(target_dir, config, qtplugin))                          