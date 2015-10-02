#!/bin/bash

if [[ ${skip.sign} == true ]]; then
    exit 0
fi

TARGET_SDK=${TARGET_SDK:-iphoneos}

/usr/bin/xcrun -sdk "${TARGET_SDK}" PackageApplication \
    -v "${libdir}/bin/${build.configuration}/${project.artifactId}.app" \
    -o "${libdir}/bin/${build.configuration}/${product.name.short}2-${project.version.name}.${project.version.code}.ipa"
