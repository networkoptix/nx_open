import sys, os, tarfile, subprocess

def extract(tar_url, extract_path='.'):
    print 'Extracting Archive: %s' % tar_url
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
    print 'Patching Artifact: %s' % tar_url
    subprocess.call([sys.executable, '-u', 'dpatcher.py', '--rules-file', 'dpatcher.cache', tar_url])
    os.unlink(file)

if __name__ == '__main__':        

    buildenv = os.path.join('${project.build.directory}', 'donotrebuild')
    if os.path.exists(buildenv): 
        os.unlink(buildenv)

    buildvar = os.path.join('${CMAKE_SOURCE_DIR}/build_variables/target', 'donotrebuild')
    if os.path.exists(buildvar): 
        os.unlink(buildvar)

    print '+++++++++++++++++++++ EXPANDING LIBS +++++++++++++++++++++'

    subprocess.call([sys.executable, 'dpatcher.py', '--make-rules-file', 'dpatcher.cache'])

    for file in os.listdir('.'):
        if file.endswith('tar.gz'):
            print(file)
            extract(file)

    if not os.path.exists(buildenv): 
        open(buildenv, 'w').close() 
    if not os.path.exists(buildvar): 
        open(buildvar, 'w').close() 
