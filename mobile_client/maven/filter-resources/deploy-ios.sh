#!/bin/bash

TARGET_SDK=${TARGET_SDK:-iphoneos}

/usr/bin/xcrun -sdk "${TARGET_SDK}" PackageApplication \
    -v "${libdir}/bin/${build.configuration}/${project.artifactId}.app" \
    -o "${product.name.short}-${project.version.name}.${project.version.code}-${build.configuration}.ipa" \
    --embed "${provisioning_profile}"
