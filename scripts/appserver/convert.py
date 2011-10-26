#!/usr/bin/env python

import os

os.system('rm protocol/[a-z]*.py')
schemas = ('resourcesEx', 'resources', 'cameras', 'layouts', 'users', 'servers', 'resourceTypes')

for schema in schemas:
    os.system('pyxbgen -m %s --binding-root protocol --default-namespace-public ../../eveClient/src/api/xsd/%s.xsd' % (schema, schema))
