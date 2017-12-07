'''
 usage:  candle.exe [-?] [-nologo] [-out outputFile] sourceFile [sourceFile ...] [@responseFile]

   -arch      set architecture defaults for package, components, etc.
              values: x86, x64, or ia64 (default: x86)
   -d<name>[=<value>]  define a parameter for the preprocessor
   -ext <extension>  extension assembly or "class, assembly"
   -fips      enables FIPS compliant algorithms
   -I<dir>    add to include search path
   -nologo    skip printing candle logo information
   -o[ut]     specify output file (default: write to current directory)
   -p<file>   preprocess to a file (or stdout if no file supplied)
   -pedantic  show pedantic messages
   -platform  (deprecated alias for -arch)
   -sfdvital  suppress marking files as Vital by default (deprecated)
   -ss        suppress schema validation of documents (performance boost) (deprecated)
   -sw[N]     suppress all warnings or a specific message ID
              (example: -sw1009 -sw1103)
   -swall     suppress all warnings (deprecated)
   -trace     show source trace for errors, warnings, and verbose messages (deprecated)
   -v         verbose output
   -wx[N]     treat all warnings or a specific message ID as an error
              (example: -wx1009 -wx1103)
   -wxall     treat all warnings as errors (deprecated)
   -? | -help this help information
'''

import os
import subprocess
import environment

def candle_executable():
    return os.path.join(environment.wix_directory, 'candle.exe')
