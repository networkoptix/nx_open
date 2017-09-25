#!/bin/bash

HOST=${1:-nxvms.com}

SED="sed -i''"

$SED "s/nxvms.com/$HOST/g" /usr/src/app/Gruntfile.js
$SED "s/nxvms.com/$HOST/g" /usr/src/app/test-customizations.json

su qa -c "xvfb-run --server-args='-screen 0 1280x1024x24' /test-by-test.sh"

RET=$?

echo Command return code: $RET

[ $RET -eq 0 ]
