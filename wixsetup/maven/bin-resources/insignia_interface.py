'''
 usage: insignia.exe [-?] [-nologo] [-o[ut] outputPath] [@responseFile]
           [-im databaseFile|-ib bundleFile|-ab bundlePath bundleWithAttachedContainerPath]
   -im        specify the database file to inscribe
   -ib        specify the bundle file from which to extract the engine.
              The engine is stored in the file specified by -o
   -ab        Reattach the engine to a bundle.
   -nologo    skip printing insignia logo information
   -o[ut]     specify output file. Defaults to databaseFile or bundleFile.
              If out is a directory name ending in '\', output to a file with
               the same name as the databaseFile or bundleFile in that directory
    -sw[N]     suppress all warnings or a specific message ID
               (example: -sw1009 -sw1103)
    -swall     suppress all warnings (deprecated)
    -v         verbose output
    -wx[N]     treat all warnings or a specific message ID as an error
               (example: -wx1009 -wx1103)
   -wxall     treat all warnings as errors (deprecated)
   -? | -help this help information
'''

import os
import subprocess
from environment import wix_directory, execute_command

def insignia_executable():
    return os.path.join(wix_directory, 'insignia.exe')

def extract_engine_command(bundle, output):
    return [insignia_executable(), '-ib', bundle, '-o', output]

def reattach_engine_command(engine, bundle, output):
    return [insignia_executable(), '-ab', engine, bundle, '-o', output]

def extract_engine(bundle, output):
    execute_command(extract_engine_command(bundle, output))

def reattach_engine(engine, bundle, output):
    execute_command(reattach_engine_command(engine, bundle, output))
