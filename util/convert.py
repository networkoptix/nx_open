import sys
import os
import subprocess
import threading

defaultProfile = "-r72 -dEPSCrop"

def convert(fname, profile):
    command = 'gswin32c -q -dNOPAUSE -dBATCH -sDEVICE=pngalpha %s -sOutputFile=%s.png %s.ai' % (profile, fname, fname)
    subprocess.call(command)

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
                
        if (not path[-2:] == 'ai'):
            continue;
            
        fname = str(path[:-3])
        thread = threading.Thread(None, convert, args=(fname, profile))
        thread.start()
        threads.append(thread)
        
    for thread in threads:
        thread.join()   
    
if __name__ == "__main__":
    main()