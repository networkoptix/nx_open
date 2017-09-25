#!/bin/bash -e

. ../environment
. ../common.sh

MODULE=cloud_portal_tests
VERSION=1.5

function stage()
{
    rm -rf stage

	cp -r $NX_PORTAL_DIR/front_end stage
    $SED "s/hg /echo hg /g" stage/Gruntfile.js
    $SED "s/cloud-prod.hdw.mx/nxvms.com/g" stage/Gruntfile.js
    $SED "s/cloud-prod.hdw.mx/nxvms.com/g" stage/test-customizations.json
    $SED "s/cloud-test.hdw.mx/nxvms.com/g" stage/Gruntfile.js
    $SED "s/cloud-test.hdw.mx/nxvms.com/g" stage/test-customizations.json
    $SED "s/--test-type'/--test-type', '--ignore-certificate-errors'/g" stage/protractor-conf.js
}

main $@
