import os, sys, posixpath, platform, subprocess, fileinput, shutil, re
from subprocess import Popen, PIPE
from os.path import dirname, join, exists, isfile
from os import listdir

template_file='template.pro'
specifics_file='${project.artifactId}-specifics.pro'
output_pro_file='${project.artifactId}.pro'
translations_dir='${basedir}/translations'
translations_target_dir='${project.build.directory}/resources/translations'
ldpath='${qt.dir}/lib'
translations=['${translation1}', '${translation2}', '${translation3}', '${translation4}', '${translation5}', '${translation6}', '${translation7}', '${translation8}', '${translation9}', '${translation10}',
              '${translation11}','${translation12}','${translation13}','${translation14}','${translation15}','${translation16}','${translation17}','${translation18}','${translation19}','${translation20}']
qml_files = [ ".qml", ".js", "qmldir" ]
os.environ["DYLD_FRAMEWORK_PATH"] = '${qt.dir}/lib'
os.environ["DYLD_LIBRARY_PATH"] = '${libdir}/lib/${build.configuration}:${arch.dir}'
os.environ["LD_LIBRARY_PATH"] = '${libdir}/lib/${build.configuration}'

def execute(commands):
    process = subprocess.Popen(commands, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = ''

    # Poll process for new output until finished
    for line in iter(process.stdout.readline, ""):
        text = line.rstrip() + '\n'
        sys.stdout.write(text)
        output += line.rstrip()

    process.wait()
    exitCode = process.returncode

    if (exitCode == 0):
        return output
    else:
        raise Exception(commands, exitCode, output)

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
    if not os.path.isdir(path):
        return

    for dirs in os.walk(path).next()[1]:
        if(dirs.endswith('win32')):
            print >> file, '\nwin*:INCLUDEPATH += %s/%s' % (path, dirs)
        else:
            print >> file, '\nINCLUDEPATH += %s/%s' % (path, dirs)

if __name__ == '__main__':
    if not os.path.exists('${project.build.directory}/build'):
        os.makedirs('${project.build.directory}/build')

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
    if "${noQmlInQrc}" == "true":
        exceptions += qml_files
    genqrc('build/${project.artifactId}.qrc', '/', ['${project.build.directory}/resources','${project.basedir}/static-resources','${customization.dir}/icons'], exceptions)
    if os.path.exists('${project.build.directory}/additional-resources'):
        genqrc('build/${project.artifactId}_additional.qrc', '/', ['${project.build.directory}/additional-resources'], exceptions)
        pro_file = open('${project.artifactId}-specifics.pro', 'a')
        print >> pro_file, 'RESOURCES += ${project.build.directory}/build/${project.artifactId}_additional.qrc'
        pro_file.close()

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
        gentext(f, '${project.build.sourceDirectory}', ['.ui'], 'FORMS += ')
        if "${noQmlInQrc}" == "true":
            gentext(f, '${project.basedir}/static-resources', qml_files, 'OTHER_FILES += ')
        gen_includepath(f, '${libdir}/include')
        f.close()

    if os.path.exists(os.path.join(r'${project.build.directory}', output_pro_file)):
        print (' ++++++++++++++++++++++++++++++++ qMake info: ++++++++++++++++++++++++++++++++')
        execute([r'${qt.dir}/bin/qmake', '-query'])
        print (' ++++++++++++++++++++++++++++++++ generating project file ++++++++++++++++++++++++++++++++')
        if '${platform}' == 'windows':
            vc_path = r'%s..\..\VC\bin' % os.getenv('${VCVars}')
            os.environ["path"] += os.pathsep + vc_path
            execute([r'${qt.dir}/bin/qmake', '-spec', '${qt.spec}', '-tp', 'vc', '-o', r'${project.build.sourceDirectory}/${project.artifactId}-${arch}.vcxproj', output_pro_file])
            execute([r'${qt.dir}/bin/qmake', '-spec', '${qt.spec}', r'CONFIG+=${build.configuration}', '-o', r'${project.build.directory}/Makefile', output_pro_file])
        elif '${platform}' in [ 'ios', 'macosx' ]:
            os.environ["DYLD_FRAMEWORK_PATH"] = ldpath
            os.environ["DYLD_LIBRARY_PATH"] = ldpath
            makefile = "Makefile.${build.configuration}"
            config = ""
            if "${platform}" == "ios":
                config = "CONFIG+=${iosTarget}"
                if "${iosTarget}" != "iphonesimulator":
                    config += " CONFIG-=iphonesimulator"
            qmake = "${qt.dir}/bin/qmake {0} -o {1} CONFIG+=${build.configuration} {2}".format(output_pro_file, makefile, config)
            print qmake
            os.system(qmake)
        else:
            qt_spec = "${qt.spec}"
            if qt_spec:
                qt_spec = "-spec {0}".format(qt_spec)
            os.system('export DYLD_FRAMEWORK_PATH=%s && export LD_LIBRARY_PATH=%s && ${qt.dir}/bin/qmake %s CONFIG+=${build.configuration} -o ${project.build.directory}/Makefile.${build.configuration} %s' % (ldpath, ldpath, qt_spec,  output_pro_file))

