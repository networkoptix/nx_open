import os, sys, posixpath, platform, subprocess, fileinput, shutil, re
from subprocess import Popen, PIPE
from os.path import dirname, join, exists, isfile
from os import listdir

sys.path.insert(0, '${root.dir}/common')
from gencomp import gencomp_cpp

template_file='template.pro'
specifics_file='${project.artifactId}-specifics.pro'
output_pro_file='${project.artifactId}.pro'
translations_dir='${basedir}/translations'
translations_target_dir='${project.build.directory}/resources/translations'
ldpath='${qt.dir}/lib'
translations=['${translation1}', '${translation2}', '${translation3}', '${translation4}', '${translation5}', '${translation6}', '${translation7}', '${translation8}', '${translation9}', '${translation10}',
              '${translation11}','${translation12}','${translation13}','${translation14}','${translation15}','${translation16}','${translation17}','${translation18}','${translation19}','${translation20}']
os.environ["DYLD_FRAMEWORK_PATH"] = '${qt.dir}/lib'
os.environ["DYLD_LIBRARY_PATH"] = '${libdir}/lib/${build.configuration}:${arch.dir}'
os.environ["LD_LIBRARY_PATH"] = '${libdir}/lib/${build.configuration}'

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

def fileIsAllowed(file, exclusions):
    for exclusion in exclusions:
        if file.endswith(exclusion):
            return False
    return True
        
def genqrc(qrcname, qrcprefix, pathes, exclusions, additions=''):
    os.path = posixpath

    qrcfile = open(qrcname, 'w')

    print >> qrcfile, '<!DOCTYPE RCC>'
    print >> qrcfile, '<RCC version="1.0">'
    print >> qrcfile, '<qresource prefix="%s">' % (qrcprefix)

    for path in pathes:
        for root, dirs, files in os.walk(path):
            parent = root[len(path) + 1:]
        
            for f in files:
                if fileIsAllowed(f, exclusions):
                    print >> qrcfile, '<file alias="%s">%s</file>' % (os.path.join(parent, f), os.path.join(root, f))
  
    print >> qrcfile, additions
    print >> qrcfile, '</qresource>'
    print >> qrcfile, '</RCC>'
  
    qrcfile.close()

def rreplace(s, old, new):
    li = s.rsplit(old, 1)
    return new.join(li)
    
def gentext(file, path, extensions, text): 
    os.path = posixpath
   
    for root, dirs, files in os.walk(path):
        parent = root[len(path) + 1:]
        
        for f in files:
            n = os.path.splitext(f)[0]
            p = os.path.join(parent, f)
            for extension in extensions:
                if not f.endswith(extension):
                    continue
                if n == 'StdAfx':
                    continue
                
                cond = ''
                if n.endswith('_win') or parent.endswith('_win'):
                    cond = 'win*:'
                elif n.endswith('_mac') or parent.endswith('_mac'):
                    cond = 'mac:'
                elif n.endswith('_linux') or parent.endswith('_linux'):
                    cond = 'linux*:'
                elif n.endswith('_unix'):
                    cond = 'unix:'
                    if(os.path.exists(rreplace(p, '_unix', '_mac'))):
                        cond += '!mac:'
                    if(os.path.exists(rreplace(p, '_unix', '_linux'))):
                        cond += '!linux*:'
                
                print >> file, '\n%s%s%s/%s' % (cond, text, path, os.path.join(parent, f))

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
    	    for translation in translations:
                if not translation:
                    continue
    	        if f.endswith('_%s.ts' % translation):
        	    if '${platform}' == 'windows':
            	        os.system('${qt.dir}/bin/lrelease %s/%s -qm %s/%s.qm' % (translations_dir, f, translations_target_dir, os.path.splitext(f)[0]))
                    else:
	                os.system('export DYLD_LIBRARY_PATH=%s && export LD_LIBRARY_PATH=%s && ${qt.dir}/bin/lrelease %s/%s -qm %s/%s.qm' % (ldpath, ldpath, translations_dir, f, translations_target_dir, os.path.splitext(f)[0]))
  
    exceptions = ['vmsclient.png', '.ai', '.svg', '.profile']
    genqrc('build/${project.artifactId}.qrc', '/', ['${project.build.directory}/resources','${project.basedir}/static-resources','${customization.dir}/icons'], exceptions)  
    
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
        qmake = os.system('${qt.dir}/bin/qmake -query')
        print (' ++++++++++++++++++++++++++++++++ qMake info: ++++++++++++++++++++++++++++++++')
        if '${platform}' == 'windows':
            vc_path = r'%s..\..\VC\bin' % os.getenv('VS110COMNTOOLS')
            print(vc_path)
            os.environ["path"] += os.pathsep + vc_path
            os.system('echo %PATH%')
            p = subprocess.Popen(r'qmake.bat %s' % output_pro_file, shell=True, stdout=PIPE)
            p = subprocess.Popen(r'${qt.dir}/bin/qmake -spec ${qt.spec} CONFIG+=${build.configuration} -o ${project.build.directory}/Makefile %s' % output_pro_file, shell=True, stdout=PIPE)
            out, err = p.communicate()
            print out
            p.wait()
            if p.returncode:  
                print "failed with code: %s" % str(p.returncode) 
                sys.exit(1)

            #os.system('${qt.dir}/bin/qmake -spec ${qt.spec} -tp vc -o ${project.build.sourceDirectory}/${project.artifactId}-${arch}.vcxproj %s' % output_pro_file)
            
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
        else:
            os.system('export DYLD_FRAMEWORK_PATH=%s && export LD_LIBRARY_PATH=%s && ${qt.dir}/bin/qmake -spec ${qt.spec} CONFIG+=${build.configuration} -o ${project.build.directory}/Makefile.${build.configuration} %s' % (ldpath, ldpath, output_pro_file))
