import os, sys, posixpath, platform, subprocess, fileinput, shutil, re
from os.path import dirname, join, exists, isfile
from os import listdir

sys.path.insert(0, '${basedir}/../common')
from gencomp import gencomp_cpp

template_file='template.pro'
specifics_file='${project.artifactId}-specifics.pro'
output_pro_file='${project.artifactId}.pro'
translations_dir='${basedir}/translations'
translations_target_dir='${project.build.directory}/resources/translations'

def execute(command):
    process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = ''

    # Poll process for new output until finished
    for line in iter(process.stdout.readline, ""):
        print line,
        output += line

    process.wait()
    exitCode = process.returncode

    if (exitCode == 0):
        return output
    else:
        raise Exception(command, exitCode, output)

def genqrc(qrcname, qrcprefix, pathes, extensions, exclusion, additions=''):
    os.path = posixpath

    qrcfile = open(qrcname, 'w')

    print >> qrcfile, '<!DOCTYPE RCC>'
    print >> qrcfile, '<RCC version="1.0">'
    print >> qrcfile, '<qresource prefix="%s">' % (qrcprefix)

    for path in pathes:
        for root, dirs, files in os.walk(path):
            parent = root[len(path) + 1:]
        
            for f in files:
                for extension in extensions:
                    if f.endswith(extension) and not f.endswith(exclusion):
                        print >> qrcfile, '<file alias="%s">%s</file>' % (os.path.join(parent, f), os.path.join(root, f))
  
    print >> qrcfile, additions
    print >> qrcfile, '</qresource>'
    print >> qrcfile, '</RCC>'
  
    qrcfile.close()

def gentext(file, path, extensions, text): 
    os.path = posixpath
   
    for root, dirs, files in os.walk(path):
        parent = root[len(path) + 1:]
        
        for dir in dirs:
            if dir.endswith('_specific')and not dir.endswith('${platform}_specific'):
                dirs.remove(dir)  
        
        for f in files:
            f_short = os.path.splitext(f)[0]
            for extension in extensions:
                if f.endswith(extension) and not f_short.endswith('_specific') and not f_short == 'StdAfx':
                #and not parent.endswith('_specific'):
                    print >> file, '\n%s%s/%s' % (text, path, os.path.join(parent, f))
                if f.endswith(extension) and f_short.endswith('${platform}_specific'):
                    print >> file, '\n%s%s/%s' % (text, path, os.path.join(parent, f))

def replace(file,searchExp,replaceExp):
    for line in fileinput.input(file, inplace=1):
        if searchExp in line:
            line = re.sub(r'%s', r'%s', line.rstrip() % (searchExp, replaceExp))
        sys.stdout.write(line)

        

def gen_includepath(file, path):      
    for dirs in os.walk(path).next()[1]:
        print >> file, '\nINCLUDEPATH += %s/%s' % (path, dirs)
                    
if __name__ == '__main__':
    if not os.path.exists('${project.build.directory}/build'):
        os.makedirs('${project.build.directory}/build') 
    gencomp_cpp(open('${project.build.sourceDirectory}/compatibility_info.cpp', 'w'))
    if not os.path.exists(translations_target_dir):
        os.makedirs(translations_target_dir) 

    if os.path.exists(translations_dir):    
        for f in listdir(translations_dir):
            if f.endswith('.ts'):
                os.system('lrelease %s/%s -qm %s/%s.qm' % (translations_dir, f, translations_target_dir, os.path.splitext(f)[0]))
  
    genqrc('build/${project.artifactId}.qrc', '/', ['${project.build.directory}/resources','${libdir}/icons'], [''],'vmsclient.png')  
    
    if os.path.exists(os.path.join(r'${project.build.directory}', template_file)):
        f = open(output_pro_file, "w")
        for file in [template_file, specifics_file]:
            if os.path.exists(file):
                fo = open(file, "r")
                f.write(fo.read())
                print >> f, '\n'
                fo.close()
        gentext(f, '${project.build.sourceDirectory}', ['.cpp', '.c'], 'SOURCES += ')
        gentext(f, '${project.build.sourceDirectory}', ['.h'], 'HEADERS += ')
        gentext(f, '${project.build.sourceDirectory}', ['.proto'], 'PB_FILES += ')
        gentext(f, '${project.build.sourceDirectory}', ['.ui'], 'FORMS += ')
        gen_includepath(f, '${libdir}/include')
        gen_includepath(f, '${environment.dir}/include')
        f.close()
    
    if os.path.exists(os.path.join(r'${project.build.directory}', output_pro_file)):
        print (' ++++++++++++++++++++++++++++++++ generating project file ++++++++++++++++++++++++++++++++')
        print (' ++++++++++++++++++++++++++++++++ qMake info: ++++++++++++++++++++++++++++++++')
        os.system('${qt.dir}/bin/qmake -query')
        if '${platform}' == 'windows':
            vc_path = r'%s..\..\VC\bin' % os.getenv('VS110COMNTOOLS')
            print(vc_path)
            os.environ["path"] += os.pathsep + vc_path
            os.system('echo %PATH%')
            os.system('${qt.dir}/bin/qmake -spec win32-msvc2012 -tp vc -o ${project.build.sourceDirectory}/${project.artifactId}-${arch}.vcxproj ${project.build.directory}/${project.artifactId}.pro')
            
            #if '${arch}' == 'x64' and '${force_x86}' == 'false':
            #    replace ('${project.build.sourceDirectory}/${project.artifactId}-${arch}.vcxproj', 'Win32', '${arch}')
            #    replace ('${project.build.sourceDirectory}/${project.artifactId}-${arch}.vcxproj', 'Name="VCLibrarianTool"', 'Name="VCLibrarianTool" \n				AdditionalOptions="/MACHINE:x64"')
            #print ('f++++++++++++++++++++++++++++++++++++++ Replacing +++++++++++++++++++++++++++++++++++++++')
            #replace ('${project.build.sourceDirectory}/${project.artifactId}-${arch}.vcxproj', '<None\s*Include=\"(.*)[\\/]{1}([\w\d_\-]+)\.([\w\d_\-]+)\".*/>', '''    <CustomBuild Include="$1/$2.$3"> \n
      #<AdditionalInputs>${libdir}/build/bin/protoc;$1/$2.$3</AdditionalInputs> \n
      #<Command         >${libdir}/build/bin/protoc --proto_path=${root}/${project.artifactId}/src/api/pb --cpp_out=${root}/${project.artifactId}/x86/build/\$(Configuration)/generated/ ${root}/${project.artifactId}/src/$1/$2.$3</Command> \n
      #<Message         >Generating code from $1/$2.$3 to $2.pb.cc</Message> \n
      #<Outputs         >${root}/${project.artifactId}/x86/build/\$(Configuration)/generated/$2.pb.cc</Outputs> \n
    #</CustomBuild>''')
        elif sys.platform == 'linux2':
            os.environ["LD_LIBRARY_PATH"] = '${qt.dir}/lib'
            execute(['${qt.dir}/bin/qmake -spec linux-g++ CONFIG+=${build.configuration} -o ${project.build.directory}/Makefile.${build.configuration} ${project.build.directory}/${project.artifactId}.pro'])
        elif sys.platform == 'darwin':
            execute(['${qt.dir}/bin/qmake -spec macx-g++47 CONFIG+=${build.configuration} -o ${project.build.directory}/Makefile.${build.configuration} ${project.build.directory}/${project.artifactId}.pro'])
