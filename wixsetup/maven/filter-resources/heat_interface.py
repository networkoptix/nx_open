'''
 usage:  heat.exe harvestType harvestSource <harvester arguments> -o[ut] sourceFile.wxs

Supported harvesting types:

   dir      harvest a directory
   file     harvest a file
   payload  harvest a bundle payload as RemotePayload
   perf     harvest performance counters
   project  harvest outputs of a VS project
   reg      harvest a .reg file
   website  harvest an IIS web site

Options:
   -ag      autogenerate component guids at compile time
   -cg <ComponentGroupName>  component group name (cannot contain spaces e.g -cg MyComponentGroup)
   -configuration  configuration to set when harvesting the project
   -directoryid  overridden directory id for generated directory elements
   -dr <DirectoryName>  directory reference to root directories (cannot contain spaces e.g. -dr MyAppDirRef)
   -ext     <extension>  extension assembly or "class, assembly"
   -g1      generated guids are not in brackets
   -generate
            specify what elements to generate, one of:
                components, container, payloadgroup, layout, packagegroup
                (default is components)
   -gg      generate guids now
   -indent <N>  indentation multiple (overrides default of 4)
   -ke      keep empty directories
   -nologo  skip printing heat logo information
   -out     specify output file (default: write to current directory)
   -platform  platform to set when harvesting the project
   -pog
            specify output group of VS project, one of:
                Binaries,Symbols,Documents,Satellites,Sources,Content
              This option may be repeated for multiple output groups.
   -projectname  overridden project name to use in variables
   -scom    suppress COM elements
   -sfrag   suppress fragments
   -srd     suppress harvesting the root directory as an element
   -sreg    suppress registry harvesting
   -suid    suppress unique identifiers for files, components, & directories
   -svb6    suppress VB6 COM elements
   -sw<N>   suppress all warnings or a specific message ID
            (example: -sw1011 -sw1012)
   -swall   suppress all warnings (deprecated)
   -t       transform harvested output with XSL file
   -template  use template, one of: fragment,module,product
   -v       verbose output
   -var <VariableName>  substitute File/@Source="SourceDir" with a preprocessor or a wix variable
(e.g. -var var.MySource will become File/@Source="$(var.MySource)\myfile.txt" and
-var wix.MySource will become File/@Source="!(wix.MySource)\myfile.txt"
   -wixvar  generate binder variables instead of preprocessor variables
   -wx[N]   treat all warnings or a specific message ID as an error
            (example: -wx1011 -wx1012)
   -wxall   treat all warnings as errors (deprecated)
'''

import os
import subprocess

wix_directory = '${wix_directory}/bin'

def heat_executable():
    return os.path.join(wix_directory, 'heat.exe')

def common_heat_options():
    return ['-wixvar', '-nologo', '-sfrag', '-suid', '-sreg', '-ag', '-srd']

def harvest_dir_command(source_dir, target_file, component_group_name, directory_ref, vars):
    command = [heat_executable(), 'dir', source_dir]
    command += common_heat_options()
    command += [
        '-out', target_file,
        '-cg', component_group_name,
        '-dr', directory_ref]
    for var in vars:
        command += ['-var', var]
    return command

def harvest_dir(source_dir, target_file, component_group_name, directory_ref, vars):
    command = harvest_dir_command(source_dir, target_file, component_group_name, directory_ref, vars)
    return subprocess.check_output(command, stderr = subprocess.STDOUT)
