#!/usr/bin/env python

import sys, getopt, os, shutil, fileinput

def time(line):
    elements = line.split(';', 3)
    if len(elements) == 2: # storage_index.csv
        try:
            return long(elements[1]) + 1 
        except ValueError:
            return 0
            
    try:
        return long(elements[1]) 
    except ValueError:
        return 0

def copyFile(sourceFile, targetFile):
    print 'Copying file', sourceFile, targetFile
    shutil.copyfile(sourceFile, targetFile)

def combineFiles(sourceFile, attachFile, targetFile):
    print 'Combining files', sourceFile, attachFile, 'into', targetFile
    
    with open(targetFile, 'w') as t:
        lines = list(fileinput.input([sourceFile, attachFile]))
        times = set()
        for line in sorted(lines, key=time):
            stamp = time(line)
            if stamp in times:
                continue;
            times.add(stamp)
            t.write(line)
    

def doAttach(source, attach, output):
    # Copying single files from the first folder and merging duplicated files 
    for root, dirs, files in os.walk(source):
        for f in files:
            targetDir = output if root == source else os.path.join(output, os.path.relpath(root, source)) 
            if not os.path.exists(targetDir):
                print 'Trying to make dir1', targetDir
                os.makedirs(targetDir)
            full = os.path.join(root, f)
            relative = os.path.relpath(full, source)
            attaching = os.path.join(attach, relative)
            target = os.path.join(output, relative)
            if (os.path.exists(attaching)):
                combineFiles(full, attaching, target)
            else:
                copyFile(full, target)
                
    # Copying single files from the second folder
    for root, dirs, files in os.walk(attach):
        for f in files:
            targetDir = output if root == attach else os.path.join(output, os.path.relpath(root, attach)) 
            if not os.path.exists(targetDir):
                print 'Trying to make dir2', targetDir
                os.makedirs(targetDir)
            full = os.path.join(root, f)
            relative = os.path.relpath(full, attach)
            origin = os.path.join(source, relative)
            target = os.path.join(output, relative)
            if not (os.path.exists(origin)):
                copyFile(full, target)

def main(argv):
    help  = 'join.py -s <source> -a <attach> -o <output>'

    source = ''
    attach = ''
    output = ''
    try:
        opts, args = getopt.getopt(argv,"hs:a:o:",["source=","attach=","output="])
    except getopt.GetoptError:
        print help
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print help
            sys.exit()
        elif opt in ("-s", "--source"):
            source = arg
        elif opt in ("-a", "--attach"):
            attach = arg         
        elif opt in ("-o", "--output"):
            output = arg
    print 'Input file is "', source, attach
    print 'Output file is "', output
    doAttach(source, attach, output)
    sys.exit(0)

if __name__ == "__main__":
    main(sys.argv[1:])


