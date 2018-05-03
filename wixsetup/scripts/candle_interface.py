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
from environment import execute_command


def candle_executable(wix_directory):
    return os.path.join(wix_directory, 'candle.exe')


def common_candle_options():
    return ['-nologo']


def candle_command(
    wix_directory,
    arch,
    components,
    variables,
    extensions,
    output_folder
):
    command = [
        candle_executable(wix_directory),
        '-arch', arch,
        '-out', output_folder + '/'  # Candle output folder must end with separator
    ]

    command += common_candle_options()

    for extension in extensions:
        command += ['-ext', '{0}.dll'.format(extension)]

    for key, value in variables.iteritems():
        command += ['-d{0}={1}'.format(key, value)]

    for component in components:
        command += ['{0}.wxs'.format(component)]

    return command


def candle(
    wix_directory,
    arch,
    components,
    variables,
    extensions,
    output_folder
):
    execute_command(candle_command(
        wix_directory=wix_directory,
        arch=arch,
        components=components,
        variables=variables,
        extensions=extensions,
        output_folder=output_folder))
