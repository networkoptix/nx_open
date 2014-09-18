#/bin/python

import sys
import os

def fileStructs(file):
    result = []
    with open(file, "r") as header:
        for line in header.readlines():
            if not 'struct ' in line:
                continue
            keywords = line.split(' ')
        
            found = False
            for word in keywords:
                if found:
                    result.append('(' + word.strip(':\n') + ')')
                    break
                    
                if (word != 'struct'):
                    continue
                found = True
    return result
        
def writeCpp(fileName, structs):
    basename = os.path.basename(fileName)
    formats = '(ubjson)(xml)(json)(sql_record)(csv_record)'
    with open(fileName + '.cpp', "w") as target:
        target.write('#include "' + basename + '.h"\n')
        target.write('#include "api_model_functions_impl.h"\n')
        target.write('\n')
        target.write('namespace ec2 {\n')
        target.write('    QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(' + ' '.join(structs) + ', ' + formats +', _Fields)\n')
        target.write('} // namespace ec2\n')
        target.close()
        
def main():
    scriptDir = os.path.dirname(os.path.abspath(__file__))

    for entry in os.listdir(scriptDir):
        path = os.path.join(scriptDir, entry)
        if (os.path.isdir(path)):
            continue
        fileName, fileExtension = os.path.splitext(path)
        if (fileExtension != '.h'):
            continue
        if ('fwd' in os.path.basename(fileName)):
            continue
        structs = fileStructs(path)
        if len(structs) == 0:
            print 'Empty file:' + fileName
            continue
        writeCpp(fileName, structs)
    sys.exit(0)

if __name__ == "__main__":
    main()
