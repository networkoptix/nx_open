#!/bin/bash

TARGET_SDK=${TARGET_SDK:-iphoneos}

IPA_SUFFIX=

if [ -f "${provisioning_profile}" ]
then
    XCRUN_PROVISION_ARGS="--embed ${provisioning_profile}"
else
    XCRUN_PROVISION_ARGS=
fi

echo "Unlocking Keychain..."
security unlock-keychain -p qweasd123 $HOME/Library/Keychains/login.keychain

/usr/bin/xcrun -sdk "${TARGET_SDK}" PackageApplication \
    -v "${libdir}/bin/${build.configuration}/${project.artifactId}.app" \
    -o "${project.build.directory}/${client_distribution_name}.ipa" \
    ${XCRUN_PROVISION_ARGS}
