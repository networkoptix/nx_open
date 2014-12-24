import io, sys, os, errno, platform, shutil, fnmatch, tarfile
from os.path import dirname, join, exists, isfile

basedir = join(dirname(os.path.abspath(__file__)))
sys.path.append(basedir + '/' + '../..')

from main import get_environment_variable, cd

qtlibs = ['${qtlib1}', '${qtlib2}', '${qtlib3}', '${qtlib4}', '${qtlib5}', '${qtlib6}', '${qtlib7}', '${qtlib8}', '${qtlib9}', '${qtlib10}', '${qtlib11}', '${qtlib12}', '${qtlib13}', '${qtlib14}', '${qtlib15}', '${qtlib16}', '${qtlib17}', '${qtlib18}', '${qtlib19}', '${qtlib20}']
qtplugins = ['${qtplugin1}', '${qtplugin2}', '${qtplugin3}']
qtbasedir = '${qt.dir}/..'

def get_platform():
    if sys.platform == 'win32':
        return 'windows'
    elif sys.platform == 'linux2':
        return 'linux'
    elif sys.platform == 'darwin':
        return 'macosx'   

def get_build_arch():
    if '${arch}'.startswith('arm'):
        return '${arch}'
    else:
        if platform.architecture()[0] == '64bit':
            return 'x64'
        else:
            return 'x86'            
        
def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc: # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else: raise

def extract(tar_url, extract_path='.'):
    print tar_url
    tar = tarfile.open(tar_url, 'r')
    for item in tar:
        #print (item.name)
        if(os.path.isfile(os.path.join(extract_path,item.name))):
            print (os.path.join(extract_path,item.name))
            os.unlink(os.path.join(extract_path,item.name));
        tar.extract(item, extract_path)
        if item.name.find(".tar.gz") != -1:
            extract(item.name, "./" + item.name[:item.name.rfind('/')])        
    tar.close() 
    os.unlink(file)
if __name__ == '__main__':        

    buildenv = join('${project.build.directory}', 'donotrebuild')
    if os.path.exists(buildenv): 
        os.unlink(buildenv)

    print '+++++++++++++++++++++ EXPANDING LIBS +++++++++++++++++++++'
            
    for file in os.listdir('.'):
        if file.endswith('tar.gz'):
            print(file)
            extract(file)

    print '+++++++++++++++++++++ COPYING QT LIBS +++++++++++++++++++++'
            
    if get_platform() == 'windows':        
        for arch in ('x86', 'x64'):
            plugin_source_dir = '%s/%s/plugins' % (qtbasedir, arch)
            lib_source_dir = '%s/%s/bin' % (qtbasedir, arch)
            pdb_source_dir = '%s/%s/lib' % (qtbasedir, arch)
            target_dir = join('${project.build.directory}', arch, 'bin')
            lib_target_dir = join('${project.build.directory}', arch, 'lib')
            help_dir = join('${project.build.directory}', arch, 'bin/help')
                
            #if os.path.exists(help_dir):
            #    shutil.rmtree(help_dir)
            #    shutil.copytree(join(basedir, 'help'), help_dir)

            if not os.path.exists(target_dir):
                mkdir_p(target_dir)        
            
            for qtlib in qtlibs:
                if qtlib != '':
                    for file in os.listdir(lib_source_dir):
                        if fnmatch.fnmatch(file, 'qt5%sd.dll' % qtlib) or fnmatch.fnmatch(file, 'qt5%sd.pdb' % qtlib):
                            print (join(lib_source_dir, file))
                            shutil.copy2(join(lib_source_dir, file), join(target_dir, 'debug'))
                        elif fnmatch.fnmatch(file, 'qt5%s.dll' % qtlib) or fnmatch.fnmatch(file, 'qt5%s.pdb' % qtlib):
                            print (join(lib_source_dir, file))
                            shutil.copy2(join(lib_source_dir, file), join(target_dir, 'release'))    
            
            for qtplugin in qtplugins:
                for config in ('debug', 'release'):
                    if not os.path.exists(join(target_dir, config, qtplugin)):
                        os.makedirs (join(target_dir, config, qtplugin))    
                
                if qtplugin != '':
                    print join(plugin_source_dir, qtplugin)
                    for file in os.listdir(join(plugin_source_dir, qtplugin)):
                        if fnmatch.fnmatch(file, 'q*d.*'):
                            shutil.copy2(join(plugin_source_dir, qtplugin, file), join(target_dir, 'debug', qtplugin))
                        else:
                            shutil.copy2(join(plugin_source_dir, qtplugin, file), join(target_dir, 'release', qtplugin))
                            
            for config in ('debug', 'release'):
                target_vox = join(target_dir, config, 'vox')
                if os.path.exists(target_vox):
                    shutil.rmtree(target_vox)            
                target_plugins = join(target_dir, config, 'plugins')
                if not os.path.exists(target_plugins):
                    os.makedirs(join(target_dir, config, 'plugins'))
                for file in os.listdir(lib_source_dir):
                    if fnmatch.fnmatch(file, 'icu*.dll') or fnmatch.fnmatch(file, 'lib*.dll'):
                        print (join(lib_source_dir, file))
                        shutil.copy2(join(lib_source_dir, file), join(target_dir, config))                    
                #shutil.copytree(join('${project.build.directory}/bin', config, 'vox'), target_vox)                        z
			
            for file in os.listdir(lib_source_dir):
                if fnmatch.fnmatch(file, '*.pdb') or fnmatch.fnmatch(file, '*d.dll'):
                    print ('++++++ DELETING %s +++++++++++' % join(lib_source_dir, file))                    
                    os.unlink(join(lib_source_dir, file))			
    else:     
        lib_source_dir = '${qt.dir}/lib'
        lib_target_dir = join('${project.build.directory}', 'lib')
        plugin_source_dir = '${qt.dir}/plugins'	
        target_dir = join('${project.build.directory}', 'bin')
        help_dir = join('${project.build.directory}', 'bin/help')
        
    #    if os.path.exists(help_dir):
    #        shutil.rmtree(help_dir)
    #        shutil.copytree('help', help_dir)    
        
        if get_platform() == 'linux':
            for qtlib in qtlibs:
                if qtlib != '': 
                    for file in os.listdir(lib_source_dir):            
                        for config in ('debug', 'release'):
                            if fnmatch.fnmatch(file, 'libQt5%s.so.*' % qtlib) or fnmatch.fnmatch(file, 'libicu*.so*'):
                                print (join(lib_source_dir, file))
                                srcname = join(lib_source_dir, file)
                                dstname = join(lib_target_dir, config, file)
                                if os.path.exists(dstname):
                                    os.unlink(dstname)
                                if os.path.islink(srcname):
                                    linkto = os.readlink(srcname)
                                    os.symlink(linkto, dstname)
                                else:
                                    shutil.copy2(srcname, dstname)       
        elif get_platform() == 'macosx':
            for qtlib in qtlibs:
                if qtlib != '': 
                    for root, dirs, files in os.walk(lib_source_dir):
                        for dir in dirs:
                            if dir.endswith('Qt%s.framework' % qtlib):
                                print (join(dir))
                                shutil.copy2(join(lib_source_dir, dir, 'Versions/5', 'Qt%s_debug' % qtlib), join(lib_target_dir, 'debug'))
                                shutil.copy2(join(lib_source_dir, dir, 'Versions/5', 'Qt%s' % qtlib), join(lib_target_dir, 'release'))                                      
        else: print '+++++++++++++++++++++++ Could not recognize platform +++++++++++++++++++++++'
                                  
        for config in ('debug', 'release'):
            for qtplugin in qtplugins:
                if qtplugin != '':
                    target = join(target_dir, config, qtplugin)
                    print target
                    if os.path.exists(target):
                        shutil.rmtree(target)
                    shutil.copytree(join(plugin_source_dir, qtplugin), target)                          
    if not os.path.exists(buildenv): 
        open(buildenv, 'w').close() 
