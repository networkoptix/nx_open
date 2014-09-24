#!/usr/bin/env python

import sys
from ConfigParser import ConfigParser, NoSectionError, NoOptionError

if len(sys.argv) < 2 or len(sys.argv) > 4:
    print "Usage: config_helper <file> | config_helper <file> name value"
    sys.exit(1)

config_file = sys.argv[1]
cfgparser = ConfigParser()
cfgparser.optionxform = str

cfgparser.read(config_file)
if len(sys.argv) == 2:
    try:
        for name, value in cfgparser.items('General'):
            print "%s='%s'" % (name, value)
    except NoSectionError:
        pass
elif len(sys.argv) == 3:
    try:
        print cfgparser.get('General', sys.argv[2])
    except NoSectionError:
        pass
    except NoOptionError:
        pass
else:
    if not cfgparser.has_section('General'):
        cfgparser.add_section('General')

    if sys.argv[2] == '-d':
        name = sys.argv[3]
        if cfgparser.has_option('General', name):
            cfgparser.remove_option('General', name)
    else:
        name, value = sys.argv[2:4]
        cfgparser.set('General', name, value)

    with open(config_file, 'w') as f:
        cfgparser.write(f)
