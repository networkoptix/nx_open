#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -e #< Stop on first error.
set -u #< Require variable definitions.

source "$(dirname $0)/../../build_distribution_utils.sh"

distrib_loadConfig "build_distribution.conf"

APK_BUILD_DIR="open/vms/client/mobile_client"
APK_DIR_NAME="mobile_client_apk"
DEPLOYMENT_SETTINGS_FILE="android-deployment-settings.json"

WORK_DIR="$CURRENT_BUILD_DIR/aab_tmp"
LOG_FILE="$LOGS_DIR/mobile_client_aab_distribution.log"
CURRENT_DEPLOYMENT_SETTINGS_FILE="$WORK_DIR/$DEPLOYMENT_SETTINGS_FILE"

QT_DIR="$WORK_DIR/qt"
APK_TEMPLATE_DIR="$WORK_DIR/mobile_client_apk_template"
APK_DIR="$WORK_DIR/$APK_DIR_NAME"
AAB_BUILD_CONF_FILE="$WORK_DIR/aab_build.conf"

declare -A FULL_ABI_NAME_BY_SHORT=(
    [arm32]=armeabi-v7a
    [arm64]=arm64-v8a
)

declare -A TOOLCHAIN_PREFIX_BY_ABI=(
    [arm32]=arm-linux-androideabi
    [arm64]=aarch64-linux-android
)

#--------------------------------------------------------------------------------------------------

readStringVariableFromJson()
{
    local -r JSON_FILE="$1"
    local -r VARIABLE="$2"

    "$PYTHON" -c "import json; print(json.load(open('$JSON_FILE'))['$VARIABLE'])"
}

# [in] QT_DIR
# [in] ANDROID_ABIS
# [in] ABI_BUILD_DIRS
# [in] APK_BUILD_DIR
# [in] DEPLOYMENT_SETTINGS_FILE
mergeQtPackages()
{
    echo "Merging Qt packages into one multi-ABI package."

    mkdir -p "$QT_DIR"

    for ABI in ${ANDROID_ABIS[@]}
    do
        ABI_DIR="${ABI_BUILD_DIRS[$ABI]}"
        SETTINGS_FILE="$ABI_DIR/$APK_BUILD_DIR/$DEPLOYMENT_SETTINGS_FILE"
        ABI_QT_DIR=$(readStringVariableFromJson "$SETTINGS_FILE" qt)

        if [ -d "$ABI_QT_DIR" ]
        then
            echo "  Copying $ABI_QT_DIR"
            cp -r "$ABI_QT_DIR"/* "$QT_DIR/"
        else
            echo "  error: Directory $ABI_QT_DIR does not exist."
            exit 1
        fi
    done
}

# [in] ANDROID_ABIS
# [in] ABI_BUILD_DIRS
# [in] APK_BUILD_DIR
# [in] APK_TEMPLATE_DIR
prepareApkTemplate()
{
    echo "Copying APK template."
    cp -r "${ABI_BUILD_DIRS[${ANDROID_ABIS[-1]}]}/$APK_BUILD_DIR/mobile_client_apk_apk_template" \
        "$APK_TEMPLATE_DIR"
}

# [in] ANDROID_ABIS
# [in] ABI_BUILD_DIRS
# [in] APK_DIR
prepareApkDir()
{
    echo "Preparing APK dir."

    mkdir -p "$APK_DIR/libs"

    while IFS="" read -r LIB
    do
        if [ -n "$LIB" ]; then
            echo "  Copying $LIB"
            cp "$LIB" "$APK_DIR/libs"
        else
            echo "  No extra Java libraries to copy"
        fi
    done <"${ABI_BUILD_DIRS[${ANDROID_ABIS[-1]}]}/open/vms/client/mobile_client/extra_java_libs.txt"

    for ABI in ${ANDROID_ABIS[@]}
    do
        ABI_DIR="${ABI_BUILD_DIRS[$ABI]}"
        FULL_ABI_NAME=${FULL_ABI_NAME_BY_SHORT[$ABI]}
        LIB="$ABI_DIR/lib/libmobile_client_$FULL_ABI_NAME.so"

        ABI_LIBS_DIR="$APK_DIR/libs/$FULL_ABI_NAME"
        mkdir -p "$ABI_LIBS_DIR"

        if [ -f "$LIB" ]
        then
            echo "  Copying $LIB"
            cp "$LIB" "$ABI_LIBS_DIR/libmobile_client_$FULL_ABI_NAME.so"
        else
            echo "  error: $LIB does not exist."
            exit 1
        fi
    done
}

# [in] QT_DIR
# [in] ANDROID_ABIS
# [in] ABI_BUILD_DIRS
# [in] APK_TEMPLATE_DIR
# [in] CURRENT_DEPLOYMENT_SETTINGS_FILE
# [in] AAB_BUILD_CONF_FILE
prepareDeploymentSettings()
{
    echo "Preparing deployment settings file."

    local -r REFERENCE_ABI_DIR="${ABI_BUILD_DIRS[$ABI]}"
    local -r REFERENCE_SETTINGS_FILE="$REFERENCE_ABI_DIR/$APK_BUILD_DIR/$DEPLOYMENT_SETTINGS_FILE"

    local -A SETTINGS=(
        [qt]="$QT_DIR"
        [android-package-source-directory]="$APK_TEMPLATE_DIR"
    )
    # We store the keys array separately because this script is run under Linux and macOS where we
    # use Bash and Zsh respectively, and they have different syntax for extracting the keys list.
    # Instead of writing different code for Bash and Zsh it's simpler to build the keys list
    # manually.
    local -a SETTINGS_KEYS=(
        qt
        android-package-source-directory
    )

    # Read trivially-copyable values.
    local -r VARS_TO_COPY=(
        sdk
        sdkBuildToolsRevision
        android-target-sdk-version
        ndk
        toolchain-prefix
        tool-prefix
        ndk-host
        stdcpp-path
        qml-root-path
        qml-importscanner-binary
        rcc-binary
    )

    for VAR in ${VARS_TO_COPY[@]}
    do
        VALUE="$(readStringVariableFromJson "$REFERENCE_SETTINGS_FILE" $VAR)"
        SETTINGS[$VAR]="$VALUE"
        SETTINGS_KEYS+=($VAR)
    done

    # Read extra libs.
    local EXTRA_LIBS=""

    for ABI_DIR in "${ABI_BUILD_DIRS[@]}"
    do
        SETTINGS_FILE="$ABI_DIR/$APK_BUILD_DIR/$DEPLOYMENT_SETTINGS_FILE"
        VALUE="$(readStringVariableFromJson "$SETTINGS_FILE" android-extra-libs)"
        EXTRA_LIBS+=",$VALUE"
    done

    SETTINGS[android-extra-libs]="${EXTRA_LIBS:1:${#EXTRA_LIBS}}"
    SETTINGS_KEYS+=(android-extra-libs)

    # Read architectures.
    local ARCHITECTURES=""
    for ABI in ${ANDROID_ABIS[@]}
    do
        ARCHITECTURES+=", \"${FULL_ABI_NAME_BY_SHORT[$ABI]}\": \"${TOOLCHAIN_PREFIX_BY_ABI[$ABI]}\""
    done
    ARCHITECTURES="${ARCHITECTURES:2:${#ARCHITECTURES}}"

    # Write down the settings file.
    echo "{" >"$CURRENT_DEPLOYMENT_SETTINGS_FILE"
    echo "    \"description\": \"This file is generated and should not be modified by hand.\"," \
        >>"$CURRENT_DEPLOYMENT_SETTINGS_FILE"

    for VAR in "${SETTINGS_KEYS[@]}"
    do
        echo "    \"$VAR\": \"${SETTINGS[$VAR]}\"," >>"$CURRENT_DEPLOYMENT_SETTINGS_FILE"
    done

    echo "    \"architectures\": { $ARCHITECTURES }," >>"$CURRENT_DEPLOYMENT_SETTINGS_FILE"
    echo "    \"application-binary\": \"mobile_client\"" >>"$CURRENT_DEPLOYMENT_SETTINGS_FILE"
    echo "}" >>"$CURRENT_DEPLOYMENT_SETTINGS_FILE"

    # Copy misc settings file.
    cp "$REFERENCE_ABI_DIR/$APK_BUILD_DIR/apk_build.conf" "$AAB_BUILD_CONF_FILE"
}

# [in] QT_DIR
# [in] AAB_BUILD_CONF_FILE
# [in] CURRENT_DEPLOYMENT_SETTINGS_FILE
executeAndroiddeployqt()
{
    echo "Running androiddeployqt."

    source "$AAB_BUILD_CONF_FILE"

    local CODE_SIGNING_ARGS=()
    if [ ! -z "$KEYSTORE_FILE" ]
    then
        CODE_SIGNING_ARGS=(
            --sign "$KEYSTORE_FILE" "$KEYSTORE_ALIAS"
            --storepass "$KEYSTORE_PASSWORD"
            --keypass "$KEYSTORE_KEY_PASSWORD"
        )
    fi

    # androiddeployqt creates APK in AAB mode too and needlessly tries to verify its contents. The
    # verification fails on zipalign tool. `|| true` in the end is a simple workaround.
    JAVA_HOME="$JAVA_HOME" "$QT_HOST_DIR/bin/androiddeployqt" --aab --release --gradle --verbose \
        "${CODE_SIGNING_ARGS[@]}" \
        --input "$CURRENT_DEPLOYMENT_SETTINGS_FILE" \
        --output "$APK_DIR" \
        || true
}

# [in] DISTRIBUTION_OUTPUT_DIR
# [in] AAB_BUILD_CONF_FILE
# [in] APK_DIR_NAME
# [in] AAB_FILE
copyFinalAab()
{
    echo "Copying the final AAb file."

    source "$AAB_BUILD_CONF_FILE"

    [ ! -d "$DISTRIBUTION_OUTPUT_DIR" ] && mkdir -p "$DISTRIBUTION_OUTPUT_DIR"

    SRC_AAB_FILE="$APK_DIR/build/outputs/bundle/release/$APK_DIR_NAME-release.aab"
    if [ -f "$SRC_AAB_FILE" ]
    then
        cp "$SRC_AAB_FILE" "$DISTRIBUTION_OUTPUT_DIR/$DISTRIBUTION_NAME.aab"
    else
        echo "error: AAB file is not found: $SRC_AAB_FILE"
        echo "Probably build has failed."
        exit 1
    fi
}

# [in] DISTRIBUTION_OUTPUT_DIR
copyApks()
{
    for ABI in ${ANDROID_ABIS[@]}
    do
        ABI_DIR="${ABI_BUILD_DIRS[$ABI]}"

        echo "  Copying APK for $ABI"
        cp "$ABI_DIR/distrib/"*.apk "$DISTRIBUTION_OUTPUT_DIR"
    done
}

buildDistribution()
{
    mergeQtPackages

    prepareApkTemplate
    prepareApkDir
    prepareDeploymentSettings

    executeAndroiddeployqt
    copyFinalAab
    copyApks
}

#--------------------------------------------------------------------------------------------------

main()
{
    distrib_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    declare -A ABI_BUILD_DIRS
    for ABI in ${ANDROID_ABIS[@]}
    do
        ABI_BUILD_DIRS[$ABI]="$CURRENT_BUILD_DIR/android_$ABI/src/android_$ABI-build"
    done

    buildDistribution
}

main "$@"
