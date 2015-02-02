import sys
import os
import subprocess
import threading

def convert(fname, profile):
    command = 'gswin32c %s -sOutputFile=%s.png %s.ai' % (profile, fname, fname)
    subprocess.call(command)

def main():
    scriptDir = os.path.dirname(os.path.abspath(__file__))
    
    profile = ''
    with open(os.path.join(scriptDir, '.profile')) as f:
        profile = f.readline()
    
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
        
    print "ok"
    
    
if __name__ == "__main__":
    main()