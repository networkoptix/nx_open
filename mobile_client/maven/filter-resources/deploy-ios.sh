#!/bin/bash

TARGET_SDK=${TARGET_SDK:-iphoneos}

/usr/bin/xcrun -sdk "${TARGET_SDK}" PackageApplication \
    -v "${libdir}/bin/${build.configuration}/${project.artifactId}.app" \
    -o "${libdir}/bin/${build.configuration}/${product.name.short}-${project.version.name}.${project.version.code}.ipa" \
    --embed "${provisioning_profile}"
