import os, sys, posixpath, platform, subprocess, fileinput, shutil, re
from subprocess import Popen, PIPE
from os.path import dirname, join, exists, isfile
from os import listdir
from sets import Set

def splitted(value, sep=' '):
    return [s for s in [s.strip() for s in value.split(sep)] if len(s) > 0]

template_file='template.pro'
skip_template_file = 'template.skip'
specifics_file='${project.artifactId}-specifics.pro'
output_pro_file='${project.artifactId}.pro'
translations_dir='${basedir}/translations'
translations_target_dir='${project.build.directory}/resources/translations'
ldpath='${qt.dir}/lib'
translations=['${defaultTranslation}'] + splitted('${additionalTranslations}')
qml_files = [ ".qml", ".js", "qmldir" ]
additional_qrc_paths = splitted("${additionalQrcPaths}", sep=',')
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

def genqrc(qrcname, qrcprefix, paths, exclusions):
    os.path = posixpath

    qrcfile = open(qrcname, 'w')

    print >> qrcfile, '<!DOCTYPE RCC>'
    print >> qrcfile, '<RCC version="1.0">'
    print >> qrcfile, '<qresource prefix="%s">' % (qrcprefix)

    aliases = Set()

    for path in paths:
        for root, dirs, files in os.walk(path):
            parent = root[len(path) + 1:]
            for f in files:
                if fileIsAllowed(f, exclusions):
                    alias = os.path.join(parent, f)
                    if not alias in aliases:
                        aliases.add(alias)
                        print >> qrcfile, '<file alias="%s">%s</file>' % (alias, os.path.join(root, f))

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
                    cond = 'win32:'
                elif n.endswith('_linux') or parent.endswith('_linux'):
                    cond = 'linux*:'
                    if os.path.exists(rreplace(p, '_linux', '_android')):
                        cond += '!android:'
                elif n.endswith('_unix'):
                    cond = 'unix:'
                    if os.path.exists(rreplace(p, '_unix', '_linux')):
                        cond += '!linux*:'
                    if os.path.exists(rreplace(p, '_unix', '_mac')):
                        cond += '!mac:'
                    if os.path.exists(rreplace(p, '_unix', '_macx')):
                        cond += '!macx:'
                    if os.path.exists(rreplace(p, '_unix', '_android')):
                        cond += '!android:'
                    if os.path.exists(rreplace(p, '_unix', '_ios')):
                        cond += '!ios:'
                elif n.endswith('_mac') or parent.endswith('_mac'):
                    cond = 'mac:'
                elif n.endswith('_macx') or parent.endswith('_macx'):
                    cond = 'macx:'
                elif n.endswith('_android') or parent.endswith('_android'):
                    cond = 'android:'
                elif n.endswith('_ios') or parent.endswith('_ios'):
                    cond = 'ios:'

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

def append_file(source_file, dest_file):
    print 'Appending file {0} to file {1}'.format(source_file, dest_file)
    if not os.path.exists(source_file):
        print 'File {0} was not found!'
        return

    with open(source_file, 'r') as src:
        with open(dest_file, 'a') as dst:
            dst.write(src.read())
            print >> dst, '\n'

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

    genqrc(
        'build/${project.artifactId}.qrc',
        '/',
        ['${customization.dir}/icons/all', '${project.build.directory}/resources']
            + additional_qrc_paths
            + ['${project.basedir}/static-resources'],
        exceptions)

    if os.path.exists('${project.build.directory}/additional-resources'):
        genqrc('build/${project.artifactId}_additional.qrc', '/', ['${project.build.directory}/additional-resources'], exceptions)
        pro_file = open('${project.artifactId}-specifics.pro', 'a')
        print >> pro_file, 'RESOURCES += ${project.build.directory}/build/${project.artifactId}_additional.qrc'
        pro_file.close()

    output_pro_path = os.path.join(r'${project.build.directory}', output_pro_file)

    # Rewrite file
    open(output_pro_file, 'w').close()

    skip_template = os.path.exists(os.path.join(r'${project.build.directory}', skip_template_file))
    if not skip_template:
        append_file(template_file, output_pro_file)

    append_file(specifics_file, output_pro_file)

    with open(output_pro_file, "a") as f:
        gentext(f, '${project.build.sourceDirectory}', ['.cpp', '.c'], 'SOURCES += ')
        gentext(f, '${project.build.sourceDirectory}', ['.h'], 'HEADERS += ')
        gentext(f, '${project.build.sourceDirectory}', ['.ui'], 'FORMS += ')
        if "${noQmlInQrc}" == "true":
            gentext(f, '${project.basedir}/static-resources', qml_files, 'OTHER_FILES += ')
        gen_includepath(f, '${libdir}/include')

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

