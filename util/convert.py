import sys
import os
import subprocess
import threading

defaultProfile = ""

def convert(sourceFile, exportFile, profile):
    subprocess.call(['inkscape', sourceFile, '-e', exportFile] + profile.split())

def main():
    #scriptDir = os.path.dirname(os.path.abspath(__file__))
    scriptDir = os.getcwd()

    profile = ''
    profileFile = os.path.join(scriptDir, '.profile');
    if os.path.isfile(profileFile):
        with open(profileFile) as f:
            profile = f.readline()

    if len(profile) == 0:
        profile = defaultProfile

    threads = []
    for entry in os.listdir(scriptDir):
        path = os.path.join(scriptDir, entry)

        if (os.path.isdir(path)):
            continue;

        if path[-3:] == '.ai':
            fname = path[:-3]
        elif path[-4:] == '.svg':
            fname = path[:-4]
        else:
            continue

        fname += '.png'
 
        thread = threading.Thread(None, convert, args=(path, fname, profile))
        thread.start()
        threads.append(thread)

    for thread in threads:
        thread.join()   

if __name__ == "__main__":
    main()
