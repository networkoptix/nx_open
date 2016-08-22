#!/bin/bash -e

IOS_CLEAN=$1
PREPARE=$2

TARGET_SDK=${TARGET_SDK:-iphoneos}
TARGET_NAME=${TARGET_NAME:-consolebuildtest}
WORKSPACE=${WORKSPACE:-HDWitness.xcworkspace}
PROJECT_BUILDDIR=${PROJECT_BUILDDIR:-$PWD/build}
PROJECT_NAME=${PROJECT_NAME:-consolebuildtest}
BUILD_CONFIGURATION=${BUILD_CONFIGURATION:-Release}
NAMESPACE_ADDITIONAL=${NAMESPACE_ADDITIONAL:-hdwitness}
BETA=${BETA:-true}
PRODUCT_NAME_NO_SPACES=${MAVEN_DISPLAY_PRODUCT_NAME// }

function prepare {
    if which rbenv > /dev/null; then eval "$(rbenv init -)"; fi

    revision=$(hg id -i)
    build_time=$(date "+%m/%d/%y %H:%M")

    info_plist="HDWitness/HDWitness/HDWitness-Info.plist"
    settings_plist="HDWitness/Settings.bundle/Root.plist"

    /usr/libexec/PlistBuddy -c "Set :CFBundleDisplayName '${MAVEN_DISPLAY_PRODUCT_NAME}'" "${info_plist}" || true

    /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier '$MAVEN_BUNDLE_IDENTIFIER'" "$info_plist" || true
    /usr/libexec/PlistBuddy -c "Set :CFBundleShortVersionString '$MAVEN_VERSION'" "$info_plist" || true
    /usr/libexec/PlistBuddy -c "Set :CFBundleVersion '${MAVEN_BUILD_NUMBER}'" "$info_plist" || true
    /usr/libexec/PlistBuddy -c "Add :BuildTime string '$build_time'" "$info_plist" || true

    /usr/libexec/PlistBuddy -c "Set :PreferenceSpecifiers:3:DefaultValue '${MAVEN_VERSION}.${MAVEN_BUILD_NUMBER} ($revision)'" "$settings_plist" || true
    /usr/libexec/PlistBuddy -c "Set :PreferenceSpecifiers:4:DefaultValue '$build_time'" "$settings_plist" || true

    python ../common/gencomp.py objc > HDWitness/HDWitness/HDWCompatibilityItems.m

    if [ -f "${PROVISIONING_PROFILE}" ]
    then
        PROVISIONING_PROFILE_UUID=`grep UUID -A1 -a ${PROVISIONING_PROFILE} | grep -o "[-A-Za-z0-9]\{36\}"`
        CODE_SIGNING_REQUIRED=YES
    else
        DEVELOPER_NAME=
        CODE_SIGNING_REQUIRED=NO
    fi

    sed -i "" "s/\${PROVISIONING_PROFILE_UUID}/${PROVISIONING_PROFILE_UUID}/g" HDWitness/HDWitness/HDWitness.xcconfig
    sed -i "" "s/\${DEVELOPER_NAME}/${DEVELOPER_NAME}/g" HDWitness/HDWitness/HDWitness.xcconfig
    sed -i "" "s/\${CODE_SIGNING_REQUIRED}/${CODE_SIGNING_REQUIRED}/g" HDWitness/HDWitness/HDWitness.xcconfig

    sed -i "" "s/\${MAVEN_MINIMUM_IOS_VERSION}/${MAVEN_MINIMUM_IOS_VERSION}/g" HDWitness/HDWitness/HDWitness.xcconfig
    sed -i "" "s/\${VALID_ARCHS}/${VALID_ARCHS}/g" HDWitness/HDWitness/HDWitness.xcconfig
    sed -i "" "s/\${PRODUCT_NAME_NO_SPACES}/${PRODUCT_NAME_NO_SPACES}/g" HDWitness/HDWitness/HDWitness.xcconfig

    pod install --no-repo-update || true
}

function build_package {
    if [ "$IOS_CLEAN" == "true" ]; then
        echo "+++++++++++++++++++++++++++++++++++++++++++++DELETING XCODE DATA+++++++++++++++++++++++++++++++++++++++++++++++++++++++"
        rm -Rf ~/Library/Developer/Xcode/DerivedData
    fi

    PROFILES_DIR="$HOME/Library/MobileDevice/Provisioning Profiles"
    [ -d "$PROFILES_DIR" ] || mkdir -p "$PROFILES_DIR"

    if [ -f "${PROVISIONING_PROFILE}" ]
    then
        cp "${PROVISIONING_PROFILE}" "$PROFILES_DIR/${PROVISIONING_PROFILE_UUID}.mobileprovision"
        XCODEBUILD_PROVISION_ARGS="PROVISIONING_PROFILE=${PROVISIONING_PROFILE_UUID}"
        XCRUN_PROVISION_ARGS="--embed ${PROVISIONING_PROFILE}"
    else
        XCODEBUILD_PROVISION_ARGS=
        XCRUN_PROVISION_ARGS=
    fi

    rm -rf build dist
    mkdir dist

    security unlock-keychain -p 123 ${HOME}/Library/Keychains/login.keychain

    touch HDWitness/HDWitness/Base.lproj/MainStoryboard*
    xcodebuild -workspace ${WORKSPACE} -scheme HDWitness -sdk "${TARGET_SDK}" -configuration "${BUILD_CONFIGURATION}" build SYMROOT="${PROJECT_BUILDDIR}/${PROJECT_NAME}" $XCODEBUILD_PROVISION_ARGS
    [ $? != 0 ] && { echo "Cancelled..."; exit 1; }

    echo Packaging
    /usr/bin/xcrun -sdk "${TARGET_SDK}" PackageApplication -v "${PROJECT_BUILDDIR}/${PROJECT_NAME}/${BUILD_CONFIGURATION}-${TARGET_SDK}/${PRODUCT_NAME_NO_SPACES}.app" -o "${PROJECT_BUILDDIR}/${NAMESPACE_ADDITIONAL}-${MAVEN_VERSION}.${MAVEN_BUILD_NUMBER}-${SUFFIX}.ipa" $XCRUN_PROVISION_ARGS  > /dev/null
}

if [ "$PREPARE" = "prepare" ]
then
    DEVELOPER_NAME=${DEVELOPER_NAME_BETA}
    PROVISIONING_PROFILE=${PROVISIONING_PROFILE_BETA}
    SUFFIX=test
    prepare
    exit 0
fi

# Build release package
DEVELOPER_NAME=${DEVELOPER_NAME_RELEASE}
PROVISIONING_PROFILE=${PROVISIONING_PROFILE_RELEASE}
SUFFIX=production
prepare
build_package

# Build beta package
DEVELOPER_NAME=${DEVELOPER_NAME_BETA}
PROVISIONING_PROFILE=${PROVISIONING_PROFILE_BETA}
SUFFIX=test
prepare
build_package
