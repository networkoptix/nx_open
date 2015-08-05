#!/usr/bin/env python
import io, sys, os, errno, platform, shutil, fnmatch, tarfile
from os.path import dirname, join, exists, isfile

basedir = join(dirname(os.path.abspath(__file__)))
sys.path.append(basedir + '/' + '../..')

from main import get_environment_variable, cd

qtlibs = ['${qtlib1}', '${qtlib2}', '${qtlib3}', '${qtlib4}', '${qtlib5}', '${qtlib6}', '${qtlib7}', '${qtlib8}', '${qtlib9}', '${qtlib10}', '${qtlib11}', '${qtlib12}', '${qtlib13}', '${qtlib14}', '${qtlib15}', '${qtlib16}', '${qtlib17}', '${qtlib18}', '${qtlib19}', '${qtlib20}']
qtplugins = ['${qtplugin1}', '${qtplugin2}', '${qtplugin3}']
qtbasedir = '${qt.dir}'

def get_platform():
    if sys.platform == 'win32':
        return 'windows'
    elif sys.platform == 'linux2':
        return 'linux'
    elif sys.platform == 'darwin':
        return 'macosx'        
        
def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc: # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else: raise

def copyDirectory(src, dest):
    try:
        shutil.copytree(src, dest)
    # Directories are the same
    except shutil.Error as e:
        print('Directory not copied. Error: %s' % e)
    # Any error saying that the directory doesn't exist
    except OSError as e:
        print('Directory not copied. Error: %s' % e)        
        
if __name__ == '__main__':        

    print '+++++++++++++++++++++ COPYING QT LIBS +++++++++++++++++++++'
            
    if get_platform() == 'windows':        
        plugin_source_dir = '%s/plugins' % (qtbasedir)
        lib_source_dir = '%s/bin' % (qtbasedir)
        target_dir = join('${project.build.directory}', 'bin')

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
            print(join(target_dir, 'qml'))
            if os.path.exists(join(target_dir, config, 'qml')):
                shutil.rmtree(join(target_dir, config, 'qml'))
            shutil.copytree(join(qtbasedir, 'qml'), join(target_dir, config, 'qml'))
        for root, dirs, files in os.walk(join(join(target_dir, 'release', 'qml'))):
            for file in files:
                if fnmatch.fnmatch(file, '*.pdb') or fnmatch.fnmatch(file, '*d.dll'):
                    print("Deleting %s" % join(root, file))
                    os.unlink(join(root, file))
    else:     
        lib_source_dir = '${qt.dir}/lib'
        lib_target_dir = join('${project.build.directory}', 'lib')
        plugin_source_dir = '${qt.dir}/plugins'	
        target_dir = join('${project.build.directory}', 'bin')      
        
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