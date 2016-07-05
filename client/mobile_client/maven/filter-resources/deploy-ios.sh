#!/bin/bash

TARGET_SDK=${TARGET_SDK:-iphoneos}

IPA_SUFFIX=
if [[ "${build.configuration}" == "release" ]]; then
    if [[ "${beta}" == "true" ]]; then
        IPA_SUFFIX="beta"
    else
        IPA_SUFFIX="release"
    fi
else
    IPA_SUFFIX="debug"
fi

/usr/bin/xcrun -sdk "${TARGET_SDK}" PackageApplication \
    -v "${libdir}/bin/${build.configuration}/${project.artifactId}.app" \
    -o "${project.build.directory}/revamped-${installer.name}-${project.version.name}.${project.version.code}-$IPA_SUFFIX.ipa" \
    --embed "${provisioning_profile}"
