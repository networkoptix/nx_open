'''
 usage:  light.exe [-?] [-b bindPath] [-nologo] [-out outputFile] objectFile [objectFile ...] [@responseFile]

   -ai        allow identical rows, identical rows will be treated as a warning (deprecated)
   -ad        allow duplicate directory identities from other libraries (deprecated)
   -au        (experimental) allow unresolved references
              (will not create a valid output) (deprecated)
   -b <path>  specify a binder path to locate all files
              (default: current directory)
              prefix the path with 'name=' where 'name' is the name of your
              named bindpath.
   -bf        bind files into a wixout (only valid with -xo option) (deprecated)
   -binder <classname>  specify a specific custom binder to use provided by
                        an extension (deprecated)
   -cultures:<cultures>  semicolon or comma delimited list of localized
             string cultures to load from .wxl files and libraries.
             Precedence of cultures is from left to right.
   -d<name>[=<value>]  define a wix variable, with or without a value.
   -dut       drop unreal tables from the output image (deprecated)
   -ext <extension>  extension assembly or "class, assembly"
   -loc <loc.wxl>  read localization strings from .wxl file
   -nologo    skip printing light logo information
   -notidy    do not delete temporary files (useful for debugging)
   -o[ut]     specify output file (default: write to current directory)
   -pedantic  show pedantic messages
   -sadmin    suppress default admin sequence actions (deprecated)
   -sadv      suppress default adv sequence actions (deprecated)
   -sloc      suppress localization
   -sma       suppress processing the data in MsiAssembly table (deprecated)
   -ss        suppress schema validation of documents (performance boost) (deprecated)
   -sts       suppress tagging sectionId attribute on rows (deprecated)
   -sui       suppress default UI sequence actions (deprecated)
   -sv        suppress intermediate file version mismatch checking (deprecated)
   -sw[N]     suppress all warnings or a specific message ID
              (example: -sw1009 -sw1103)
   -swall     suppress all warnings (deprecated)
   -usf <output.xml>  unreferenced symbols file
   -v         verbose output
   -wx[N]     treat all warnings or a specific message ID as an error
              (example: -wx1009 -wx1103)
   -wxall     treat all warnings as errors (deprecated)
   -xo        output wixout format instead of MSI format
   -? | -help this help information

Binder arguments:
   -bcgg      use backwards compatible guid generation algorithm
              (almost never needed)
   -cc <path> path to cache built cabinets (will not be deleted after linking)
   -ct <N>    number of threads to use when creating cabinets
              (default: %NUMBER_OF_PROCESSORS%)
   -cub <file.cub> additional .cub file containing ICEs to run
   -dcl:level set default cabinet compression level
              (low, medium, high, none, mszip; mszip default)
   -eav       exact assembly versions (breaks .NET 1.1 RTM compatibility)
   -fv        add a 'fileVersion' entry to the MsiAssemblyName table
              (rarely needed)
   -ice:<ICE>   run a specific internal consistency evaluator (ICE)
   -O1        optimize smart cabbing for smallest cabinets (deprecated)
   -O2        optimize smart cabbing for faster install time (deprecated)
   -pdbout <output.wixpdb>  save the WixPdb to a specific file
              (default: same name as output with wixpdb extension)
   -reusecab  reuse cabinets from cabinet cache
   -sa        suppress assemblies: do not get assembly name information
              for assemblies
   -sacl      suppress resetting ACLs
              (useful when laying out image to a network share)
   -sf        suppress files: do not get any file information
              (equivalent to -sa and -sh)
   -sh        suppress file info: do not get hash, version, language, etc
   -sice:<ICE>  suppress an internal consistency evaluator (ICE)
   -sl        suppress layout
   -spdb      suppress outputting the WixPdb
   -spsd      suppress patch sequence data in patch XML to decrease bundle
              size and increase patch applicability performance
              (patch packages themselves are not modified)
   -sval      suppress MSI/MSM validation

Environment variables:
   WIX_TEMP   overrides the temporary directory used for cab creation, msm exploding, ...
'''

import os
import subprocess
import environment

wix_pdb = 'wixsetup.wixpdb'
light_cultures = '-cultures:{}'.format(environment.installer_cultures)
light_locale = 'OptixTheme_{}.wxl'.format(environment.installer_language)
light_cache_path = 'cab'

def light_executable():
    return os.path.join(environment.wix_directory, 'light.exe')

def common_light_options():
    return ['-sice:ICE07', '-sice:ICE38', '-sice:ICE43', '-sice:ICE57',
        '-sice:ICE60', '-sice:ICE64', '-sice:ICE69', '-sice:ICE91']

def light_command(output_file, input_folder, extensions):
    command = [light_executable(),
        light_cultures,
        '-cc', light_cache_path, '-reusecab',
        '-loc', light_locale,
        '-out', output_file,
        '-pdbout', 'obj/{}'.format(wix_pdb),
        '{}/*.wixobj'.format(input_folder)
        ]
    command += common_light_options()
    for extension in extensions:
        command += ['-ext', '{0}.dll'.format(extension)]
    
    return command

def light(output_file, input_folder, extensions):
    environment.execute_command(light_command(output_file, input_folder, extensions))
