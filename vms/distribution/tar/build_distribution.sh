#!/bin/bash
set -e #< Exit on any error.
set -u #< Prohibit undefined variables.

source "$(dirname $0)/../build_distribution_utils.sh"

distrib_loadConfig "build_distribution.conf"

WORK_DIR="build_distribution_tmp"
LOG_FILE="$LOGS_DIR/build_distribution.log"

# VMS files will be copied to this path.
VMS_INSTALL_PATH="opt/$CUSTOMIZATION"

# If not empty, a symlink will be created from this path to $VMS_INSTALL_PATH.
SYMLINK_INSTALL_PATH=""

# To save storage space on the device, some .so files can be copied to an alternative location
# defined by ALT_LIB_INSTALL_PATH (e.g. an sdcard which does not support symlinks), and symlinks
# to these .so files will be created in the regular STAGE_LIB.
ALT_LIB_INSTALL_PATH=""

if [ "$BOX" = "edge1" ]
then
    VMS_INSTALL_PATH="usr/local/apps/$CUSTOMIZATION"
    SYMLINK_INSTALL_PATH="opt/$CUSTOMIZATION"
    ALT_LIB_INSTALL_PATH="sdcard/${CUSTOMIZATION}_service"
fi

if [ "$BOX" = "bpi" ]
then
    LIB_INSTALL_PATH="$VMS_INSTALL_PATH/lib"
else
    LIB_INSTALL_PATH="$VMS_INSTALL_PATH/mediaserver/lib"
fi

#--------------------------------------------------------------------------------------------------

# Copy the specified file to the proper location, creating the symlink if necessary: if alt_lib_dir
# is empty, or the file is a symlink, just copy it to lib_dir; otherwise, put it to alt_lib_dir,
# and create a symlink from lib_dir to the file basename in symlink_target_dir.
#
copyLib() # file lib_dir alt_lib_dir symlink_target_dir
{
    local -r FILE="$1"; shift
    local -r LIB_DIR="$1"; shift
    local -r ALT_LIB_DIR="$1"; shift
    local -r SYMLINK_TARGET_DIR="$1"; shift

    if [ ! -z "$ALT_LIB_DIR" ] && [ ! -L "$FILE" ]
    then
        # FILE is not a symlink - put to the alt location, and create a symlink to it.
        cp -r "$FILE" "$ALT_LIB_DIR/"
        ln -s "$SYMLINK_TARGET_DIR/$(basename "$FILE")" \
            "$LIB_DIR/$(basename "$FILE")"
    else
        # FILE is a symlink, or the alt location is not defined - put to the regular location.
        cp -r "$FILE" "$LIB_DIR/"
    fi
}

#--------------------------------------------------------------------------------------------------

# [in] LITE_CLIENT
# [in] STAGE_LIB
# [in] ALT_LIB_INSTALL_DIR
# [in] ALT_LIB_INSTALL_PATH
copyBuildLibs()
{
    local LIBS_TO_COPY=(
        # vms
        libappserver2
        libcloud_db_client
        libcommon
        libnx_vms_server
        libnx_vms_server_db
        libnx_email
        libnx_fusion
        libnx_kit
        libnx_network
        libnx_update
        libnx_utils
        libnx_sql
        libnx_vms_api
        libnx_sdk
        libnx_plugin_utils

        # ffmpeg
        libavcodec
        libavdevice
        libavfilter
        libavformat
        libavutil
        libswresample
        libswscale

        # third-party
        libquazip
        libsigar
        libudt
    )

    local OPTIONAL_LIBS_TO_COPY=(
        libvpx
    )

    if [ "$BOX" != "edge1" ]
    then
        LIBS_TO_COPY+=(
            libnx_speech_synthesizer
        )
    fi

    # Libs for BananaPi-based Nx1 platform.
    if [ "$BOX" = "bpi" ] && [ $LITE_CLIENT_SUPPORTED = 1 ]
    then
        LIBS_TO_COPY+=(
            libGLESv2
            libMali
            libUMP
        )
    fi

    # Libs for lite client.
    if [ "$BOX" = "bpi" ] && [ $LITE_CLIENT = 1 ]
    then
        LIBS_TO_COPY+=(
            # vms
            libnx_audio
            libnx_vms_client_core
            libnx_media
            libnx_vms_utils

            # third-party
            ldpreloadhook
            libcedrus
            libopenal
            libpixman-1
            libproxydecoder
            libvdpau_sunxi
            libEGL
            libGLESv1_CM
        )
    fi

    # OpenSSL.
    LIBS_TO_COPY+=(
        libssl
        libcrypto
    )

    if [ "$BOX" = "edge1" ]
    then
        LIBS_TO_COPY+=(
            liblber
            libldap
            libsasl2
        )
    fi

    local LIB
    for LIB in "${LIBS_TO_COPY[@]}"
    do
        local FILE
        for FILE in "$BUILD_DIR/lib/$LIB".so*
        do
            if [[ $FILE != *.debug ]]
            then
                echo "Copying $(basename "$FILE")"
                copyLib "$FILE" "$STAGE_LIB" "$ALT_LIB_INSTALL_DIR" "/$ALT_LIB_INSTALL_PATH"
            fi
        done
    done
    for LIB in "${OPTIONAL_LIBS_TO_COPY[@]}"
    do
        local FILE
        for FILE in "$BUILD_DIR/lib/$LIB".so*
        do
            if [ -f "$FILE" ]
            then
                echo "Copying (optional) $(basename "$FILE")"
                copyLib "$FILE" "$STAGE_LIB" "$ALT_LIB_INSTALL_DIR" "/$ALT_LIB_INSTALL_PATH"
            fi
        done
    done
}

# [in] LITE_CLIENT
# [in] STAGE_LIB
# [in] ALT_LIB_INSTALL_DIR Absolute path for alternative lib directory in the stage.
# [in] ALT_LIB_INSTALL_PATH Alternative lib path relative to the device root.
copyQtLibs()
{
    echo ""
    echo "Copying Qt libs"

    local QT_LIBS_TO_COPY=(
        Concurrent
        Core
        Gui
        Xml
        XmlPatterns
        Network
        Multimedia
        Sql
        WebSockets
        Qml
    )

    # Qt libs for nx1 lite client.
    if [ "$BOX" = "bpi" ] && [ $LITE_CLIENT = 1 ]
    then
        QT_LIBS_TO_COPY+=(
            WebChannel
            WebEngine
            WebEngineCore
            WebView
            MultimediaQuick_p
            Quick
            LabsTemplates
            DBus
            EglDeviceIntegration
        )
    fi

    local QT_LIB
    for QT_LIB in "${QT_LIBS_TO_COPY[@]}"
    do
        local LIB_FILENAME="libQt5$QT_LIB.so"
        echo "  Copying (Qt) $LIB_FILENAME"
        local FILE
        for FILE in "$QT_DIR/lib/$LIB_FILENAME"*
        do
            copyLib "$FILE" "$STAGE_LIB" "$ALT_LIB_INSTALL_DIR" "/$ALT_LIB_INSTALL_PATH"
        done
    done
}

# [in] STAGE_MEDIASERVER_BIN
copyBins()
{
    mkdir -p "$STAGE_MEDIASERVER_BIN"
    local -r BINS_TO_COPY=(
        mediaserver
        external.dat
    )
    local BIN
    for BIN in "${BINS_TO_COPY[@]}"
    do
        local FILE
        for FILE in "$BUILD_DIR/bin/$BIN"
        do
            echo "Copying (binary) $(basename "$FILE")"
            cp -r "$FILE" "$STAGE_MEDIASERVER_BIN/"
        done
    done

    if [ "$BOX" = "bpi" ]
    then
        echo "Creating symlink for rpath needed by mediaserver binary"
        ln -s "../lib" "$STAGE_VMS/mediaserver/lib"
    fi

    echo "Copying qt.conf"
    cp -r "$CURRENT_BUILD_DIR/qt.conf" "$STAGE_MEDIASERVER_BIN/"
}

# [in] INSTALL_DIR
copyQtPlugins()
{
    echo ""
    echo "Copying Qt plugins for mediaserver"

    local -r QT_PLUGINS_INSTALL_DIR="$STAGE_VMS/mediaserver/plugins"
    mkdir -p "$QT_PLUGINS_INSTALL_DIR"

    local -r PLUGINS=(
        sqldrivers/libqsqlite.so
    )

    for PLUGIN in "${PLUGINS[@]}"
    do
        echo "  Copying (Qt plugin) $PLUGIN"

        mkdir -p "$QT_PLUGINS_INSTALL_DIR/$(dirname $PLUGIN)"
        cp -r "$QT_DIR/plugins/$PLUGIN" "$QT_PLUGINS_INSTALL_DIR/$PLUGIN"
    done
}

# [in] STAGE_MEDIASERVER_BIN
copyMediaserverPlugins()
{
    echo ""
    echo "Copying mediaserver plugins"

    if [ "$BOX" = "edge1" ]
    then
        # NOTE: Plugins from $BUILD_DIR/bin/plugins are not needed on edge1.
        local -r PLUGIN="libcpro_ipnc_plugin.so.1.0.1"
        echo "  Copying $PLUGIN"
        mkdir -p "$STAGE_MEDIASERVER_BIN/plugins"
        cp -r "$BUILD_DIR/lib/$PLUGIN" "$STAGE_MEDIASERVER_BIN/plugins/"
        return
    fi

    distrib_copyMediaserverPlugins "plugins" "$STAGE_MEDIASERVER_BIN" "${SERVER_PLUGINS[@]}"
    distrib_copyMediaserverPlugins "plugins_optional" "$STAGE_MEDIASERVER_BIN" \
        "${SERVER_PLUGINS_OPTIONAL[@]}"
}

# [in] STAGE_VMS
copyConf()
{
    if [ "$BOX" = "edge1" ]
    then
        local -r FILE="mediaserver.conf"
    else
        local -r FILE="mediaserver.conf.template"
    fi

    mkdir -p "$STAGE_VMS/mediaserver/etc"
    echo "Copying $FILE"
    cp "$CURRENT_BUILD_DIR/opt/networkoptix/mediaserver/etc/$FILE" "$STAGE_VMS/mediaserver/etc/"
}

# Copy the autostart script and platform-specific scripts.
# [in] STAGE
# [in] STAGE_VMS
copyScripts()
{
    if [ "$BOX" = "edge1" ]
    then
        mkdir -p "$STAGE/etc/init.d"
        echo "Copying customized S99networkoptix-mediaserver"
        install -m 755 "$CURRENT_BUILD_DIR/etc/init.d/S99networkoptix-mediaserver" \
            "$STAGE/etc/init.d/S99$CUSTOMIZATION-mediaserver"
    else
        echo "Copying /etc/init.d"
        cp -r "$CURRENT_BUILD_DIR/etc" "$STAGE"
        chmod -R 755 "$STAGE/etc/init.d"

        echo "Copying customized opt/networkoptix/*"
        cp -r "$CURRENT_BUILD_DIR/opt/networkoptix/"* "$STAGE_VMS/"
        local -r SCRIPTS_DIR="$STAGE_VMS/mediaserver/var/scripts"
        [ -d "$SCRIPTS_DIR" ] && chmod -R 755 "$SCRIPTS_DIR"

        NON_CUSTOMIZED_MEDIASERVER_STARTUP_SCRIPT="$STAGE/etc/init.d/networkoptix-mediaserver"
        MEDIASERVER_STARTUP_SCRIPT="$STAGE/etc/init.d/$CUSTOMIZATION-mediaserver"
        NON_CUSTOMIZED_LITE_CLIENT_STARTUP_SCRIPT="$STAGE/etc/init.d/networkoptix-lite-client"
        LITE_CLIENT_STARTUP_SCRIPT="$STAGE/etc/init.d/$CUSTOMIZATION-lite-client"

        if [ ! "$CUSTOMIZATION" = "networkoptix" ]
        then
            if [ -f "$NON_CUSTOMIZED_MEDIASERVER_STARTUP_SCRIPT" ]
            then
                mv "$NON_CUSTOMIZED_MEDIASERVER_STARTUP_SCRIPT" "$MEDIASERVER_STARTUP_SCRIPT"
            fi
            if [ -f "$NON_CUSTOMIZED_LITE_CLIENT_STARTUP_SCRIPT" ]
            then
                mv "$NON_CUSTOMIZED_LITE_CLIENT_STARTUP_SCRIPT" "$LITE_CLIENT_STARTUP_SCRIPT"
            fi
        fi
    fi
}

# [in] STAGE
copyDebs()
{
    local -r DEBS_DIR="$BUILD_DIR/deb"
    if [ -d "$DEBS_DIR" ]
    then
        echo "Copying .deb packages"
        cp -r "$DEBS_DIR" "$STAGE/opt/"
    fi
}

# [in] STAGE_VMS
# [in] STAGE_LIB
copyBpiLiteClient()
{
    local -r STAGE_LITE_CLIENT_BIN="$STAGE_VMS/lite_client/bin"

    if [ -d "$BUILD_DIR/lib/ffmpeg" ]
    then
        echo "Copying libs of a dedicated ffmpeg for Lite Client's proxydecoder"
        cp -r "$BUILD_DIR/lib/ffmpeg" "$STAGE_LIB/"
    fi

    echo "Copying mobile_client binary"
    mkdir -p "$STAGE_LITE_CLIENT_BIN"
    cp "$BUILD_DIR/bin/mobile_client" "$STAGE_LITE_CLIENT_BIN/"

    echo "Creating symlink for rpath needed by mobile_client binary"
    ln -s "../lib" "$STAGE_VMS/lite_client/lib"

    echo "Creating symlink for rpath needed by Qt plugins"
    ln -s "../../lib" "$STAGE_LITE_CLIENT_BIN/lib"

    # Copy directories needed for lite client.
    local DIRS_TO_COPY=(
        fonts #< packages/any/roboto-fonts/bin/
        egldeviceintegrations #< $QT_DIR/plugins/
        imageformats #< $QT_DIR/plugins/
        platforms #< $QT_DIR/plugins/
        qml #< $QT_DIR/
        libexec #< $QT_DIR/
    )
    local DIR
    for DIR in "${DIRS_TO_COPY[@]}"
    do
        echo "Copying directory (to Lite Client bin/) $DIR"
        cp -r "$BUILD_DIR/bin/$DIR" "$STAGE_LITE_CLIENT_BIN/"
    done

    echo "Copying Qt translations"
    cp -r "$QT_DIR/translations" "$STAGE_LITE_CLIENT_BIN/"

    # TODO: Investigate how to get rid of "resources" duplication.
    echo "Copying Qt resources"
    cp -r "$QT_DIR/resources" "$STAGE_LITE_CLIENT_BIN/"
    cp -r "$QT_DIR/resources/"* "$STAGE_LITE_CLIENT_BIN/libexec/"

    echo "Copying qt.conf"
    cp -r "$SOURCE_DIR/common/maven/bin-resources/resources/qt/etc/qt.conf" \
        "$STAGE_LITE_CLIENT_BIN/"
}

# [in] STAGE
copyBpiSpecificFiles()
{
    if [ -d "$BUILD_DIR/root" ]
    then
        echo "Copying (bpi) uboot files (linux kernel upgrade) to root/"
        cp -r "$BUILD_DIR/root" "$STAGE/"
    fi

    if [ -d "$BUILD_DIR/usr" ]
    then
        echo "Copying (bpi) usr/"
        cp -r "$BUILD_DIR/usr" "$STAGE/"
    fi

    echo "Copying (bpi) root/"
    cp -r "$CURRENT_BUILD_DIR/root" "$STAGE/"

    local -r TOOLS_PATH="root/tools/$CUSTOMIZATION"
    local -r TOOLS_DIR="$STAGE/$TOOLS_PATH"
    local -r CONF_FILE="mediaserver.conf.template"
    echo "Copying $CONF_FILE (used for factory reset) to $TOOLS_PATH/"
    mkdir -p "$TOOLS_DIR"
    cp "$CURRENT_BUILD_DIR/opt/networkoptix/mediaserver/etc/$CONF_FILE" "$TOOLS_DIR/"
}

copyEdge1SpecificFiles()
{
    local -r GDB_DIR="$STAGE_VMS/mediaserver/bin"
    echo "Copying gdb to $GDB_DIR/"
    cp -r "$PLATFORM_PACKAGES_DIR/gdb"/* "$GDB_DIR/"
}

# [in] STAGE_VMS
# [in] STAGE_LIB
copyAdditionalSysrootFilesIfNeeded()
{
    local -r SYSROOT_LIB_DIR="$SYSROOT_DIR/usr/lib/arm-linux-gnueabihf"
    local -r SYSROOT_SBIN_DIR="$SYSROOT_DIR/sbin"

    if [ "$BOX" = "bpi" ] && [ $LITE_CLIENT_SUPPORTED = 1 ]
    then
        local SYSROOT_LIBS_TO_COPY=(
            libopus
            libvpx
            libwebpdemux
            libwebp
        )
        local LIB
        for LIB in "${SYSROOT_LIBS_TO_COPY[@]}"
        do
            echo "Copying (sysroot) $LIB"
            cp -r "$SYSROOT_LIB_DIR/$LIB"* "$STAGE_LIB/"
        done
    elif [ "$BOX" = "bananapi" ]
    then
        echo "Copying (sysroot) libglib required for bananapi on Debian 8 \"Jessie\""
        cp -r "$SYSROOT_LIB_DIR/libglib"* "$STAGE_LIB/"
        echo "Copying (sysroot) hdparm required for bananapi on Debian 8 \"Jessie\""
        cp -r "$SYSROOT_SBIN_DIR/hdparm" "$STAGE_VMS/mediaserver/bin/"
    elif [ "$BOX" = "rpi" ]
    then
        echo "Copying (sysroot) hdparm"
        cp -r "$SYSROOT_SBIN_DIR/hdparm" "$STAGE_VMS/mediaserver/bin/"
    fi
}

# [in] STAGE_VMS
# [in] VOX_SOURCE_DIR
copyFestivalVox()
{
    if [ -d "$VOX_SOURCE_DIR" ]
    then
        local -r VOX_INSTALL_DIR="$STAGE_VMS/mediaserver/bin/vox"
        echo "Copying Festival Vox files"
        mkdir -p "$VOX_INSTALL_DIR"
        cp -r "$VOX_SOURCE_DIR/"* "$VOX_INSTALL_DIR/"
    fi
}

# [in] STAGE_LIB
copyToolchainLibs()
{
    echo "Copying toolchain libs (libstdc++, libatomic)"
    distrib_copySystemLibs "$STAGE_LIB" "libstdc++.so.6" "libgcc_s.so.1" "libatomic.so.1"
}

# [in] STAGE_LIB
copySystemLibs()
{
    echo "Copying system libs"

    local LIBS_TO_COPY=()

    if [ "$BOX" = "rpi" ]
    then
        LIBS_TO_COPY+=(
            libasound.so.2
            libmmal_core.so
            libmmal_util.so
            libmmal_vc_client.so
            libvchiq_arm.so
            libvcos.so
            libvcsm.so
            libbcm_host.so
        )
    fi

    for LIB in "${LIBS_TO_COPY[@]}"
    do
        echo "  Copying $LIB"
        distrib_copySystemLibs "$STAGE_LIB" "$LIB"
    done
}

# [in] WORK_DIR
# [in] STAGE
createDistributionArchive()
{
    echo ""
    echo "Creating distribution .tar.gz"
    if [ ! -z "$SYMLINK_INSTALL_PATH" ]
    then
        mkdir -p "$STAGE/$(dirname "$SYMLINK_INSTALL_PATH")"
        ln -s "/$VMS_INSTALL_PATH" "$STAGE/$SYMLINK_INSTALL_PATH"
    fi
    distrib_createArchive "$DISTRIBUTION_OUTPUT_DIR/$DISTRIBUTION_TAR_GZ" "$STAGE" tar czf
}

# [in] WORK_DIR
# [in] STAGE
createUpdateZip() # file.tar.gz
{
    local -r TAR_GZ_FILE="$1" && shift

    echo ""
    echo "Creating \"update\" .zip"
    local -r ZIP_DIR="$WORK_DIR/zip"
    mkdir -p "$ZIP_DIR"

    ln -s "$TAR_GZ_FILE" "$ZIP_DIR/"
    cp -r "$CURRENT_BUILD_DIR/update.json" "$ZIP_DIR/"
    cp -r "$CURRENT_BUILD_DIR/install.sh" "$ZIP_DIR/"

    if [ "$BOX" = "rpi" ]
    then
        cp -r "$CURRENT_BUILD_DIR/nx_rpi_cam_setup.sh" "$ZIP_DIR/"
    fi

    distrib_createArchive "$DISTRIBUTION_OUTPUT_DIR/$UPDATE_ZIP" "$ZIP_DIR" zip
}

# [in] WORK_DIR
createDebugSymbolsArchive()
{
    echo ""
    echo "Creating debug symbols .tar.gz"

    local -r TAR_GZ_DIR="$WORK_DIR/debug-symbols_tar_gz"
    mkdir -p "$TAR_GZ_DIR"

    local -i DEBUG_FILES_EXIST=0
    local FILE
    for FILE in \
        "$BUILD_DIR/lib"/*.debug \
        "$BUILD_DIR/bin"/*.debug \
        "$BUILD_DIR/bin/plugins"/*.debug \
        "$BUILD_DIR/bin/plugins_optional"/*.debug
    do
        if [ -f "$FILE" ]
        then
            DEBUG_FILES_EXIST=1
            echo "  Copying $(basename $FILE)"
            cp -r "$FILE" "$TAR_GZ_DIR/"
        fi
    done

    if [ $DEBUG_FILES_EXIST = 0 ]
    then
        echo "  No .debug files found"
    else
        distrib_createArchive \
            "$DISTRIBUTION_OUTPUT_DIR/$DISTRIBUTION_TAR_GZ-debug-symbols.tar.gz" "$TAR_GZ_DIR" \
            tar czf
    fi
}

#--------------------------------------------------------------------------------------------------

# [in] LITE_CLIENT
# [in] WORK_DIR
buildDistribution()
{
    local -r STAGE="$WORK_DIR/tar"
    local -r STAGE_VMS="$STAGE/$VMS_INSTALL_PATH"
    local -r STAGE_LIB="$STAGE/$LIB_INSTALL_PATH"
    local -r STAGE_MEDIASERVER_BIN="$STAGE_VMS/mediaserver/bin"

    mkdir -p "$STAGE_VMS"
    mkdir -p "$STAGE_LIB"
    if [ -z "$ALT_LIB_INSTALL_PATH" ]
    then
        local -r ALT_LIB_INSTALL_DIR=""
    else
        local -r ALT_LIB_INSTALL_DIR="$STAGE/$ALT_LIB_INSTALL_PATH"
        mkdir -p "$ALT_LIB_INSTALL_DIR"
    fi

    echo "Generating version.txt: $VERSION"
    echo "$VERSION" >"$STAGE_VMS/version.txt"

    echo "Copying build_info.txt"
    cp -r "$BUILD_DIR/build_info.txt" "$STAGE_VMS/"
    cp -r "$BUILD_DIR/specific_features.txt" "$STAGE_VMS/"

    copyBuildLibs
    copyQtLibs
    copyBins
    copyQtPlugins
    copyMediaserverPlugins
    copyConf
    copyScripts
    copyDebs
    copyAdditionalSysrootFilesIfNeeded
    copyFestivalVox
    copyToolchainLibs
    copySystemLibs

    if [ "$BOX" = "bpi" ]
    then
        copyBpiSpecificFiles
        if [ $LITE_CLIENT = 1 ]
        then
            copyBpiLiteClient
        fi
    elif [ "$BOX" = "edge1" ]
    then
        copyEdge1SpecificFiles
    fi

    createDistributionArchive
    createUpdateZip "$DISTRIBUTION_OUTPUT_DIR/$DISTRIBUTION_TAR_GZ"
    cp "$DISTRIBUTION_OUTPUT_DIR/$UPDATE_ZIP" "$DISTRIBUTION_OUTPUT_DIR/$DISTRIBUTION_ZIP"

    createDebugSymbolsArchive
}

#--------------------------------------------------------------------------------------------------

# [out] LITE_CLIENT
parseExtraArgs() # arg arg_n "$@"
{
    if [[ $1 = "--no-client" ]]
    then
        LITE_CLIENT=0
    fi
}

main()
{
    local -r LITE_CLIENT_SUPPORTED=0
    local -i LITE_CLIENT=1
    distrib_EXTRA_HELP=" --no-client: Do not pack Lite Client."
    distrib_PARSE_EXTRA_ARGS_FUNC=parseExtraArgs

    distrib_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    buildDistribution
}

main "$@"
