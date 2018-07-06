#!/bin/bash
set -e #< Exit on any error.
set -u #< Prohibit undefined variables.

source "$(dirname $0)/../build_utils/linux/build_distribution_utils.sh"

distr_loadConfig "build_distribution.conf"

LIB_BUILD_DIR="$BUILD_DIR/lib"
BIN_BUILD_DIR="$BUILD_DIR/bin"

# VMS files will be copied to this path.
INSTALL_PATH="opt/$CUSTOMIZATION"

# If not empty, a symlink will be created from this path to $INSTALL_PATH.
SYMLINK_INSTALL_PATH=""

# To save storage space on the device, some .so files can be copied to an alternative location
# defined by ALT_LIB_INSTALL_PATH (e.g. an sdcard which does not support symlinks), and symlinks
# to these .so files will be created in the regular LIB_INSTALL_DIR.
ALT_LIB_INSTALL_PATH=""

OUTPUT_DIR=${DISTRIBUTION_OUTPUT_DIR:-"$CURRENT_BUILD_DIR"}

if [ "$BOX" = "edge1" ]
then
    INSTALL_PATH="usr/local/apps/$CUSTOMIZATION"
    SYMLINK_INSTALL_PATH="opt/$CUSTOMIZATION"
    ALT_LIB_INSTALL_PATH="sdcard/${CUSTOMIZATION}_service"
fi

if [ "$BOX" = "bpi" ]
then
    LIB_INSTALL_PATH="$INSTALL_PATH/lib"
else
    LIB_INSTALL_PATH="$INSTALL_PATH/mediaserver/lib"
fi

LOG_FILE="$LOGS_DIR/create_arm_installer.log"

WORK_DIR="create_arm_installer_tmp"

#--------------------------------------------------------------------------------------------------

createArchive() # archive dir command...
{
    local -r ARCHIVE="$1"; shift
    local -r DIR="$1"; shift

    rm -rf "$ARCHIVE" #< Avoid updating an existing archive.
    echo "  Creating $ARCHIVE"
    ( cd "$DIR" && "$@" "$ARCHIVE" * ) #< Subshell prevents "cd" from changing the current dir.
    echo "  Done"
}

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

# [in] Library name
# [in] Destination directory
copy_sys_lib()
{
    "$SOURCE_DIR"/build_utils/copy_system_library.sh -c "$COMPILER" "$@"
}

#--------------------------------------------------------------------------------------------------

# [in] LITE_CLIENT
# [in] LIB_INSTALL_DIR
# [in] ALT_LIB_INSTALL_DIR
# [in] ALT_LIB_INSTALL_PATH
copyBuildLibs()
{
    local LIBS_TO_COPY=(
        # vms
        libappserver2
        libcloud_db_client
        libcommon
        libmediaserver_core
        libmediaserver_db
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
        libpostproc

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
    if [ "$BOX" = "bpi" ]
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
            libnx_client_core
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

    # OpenSSL (for latest debians).
    if [ "$BOX" = "rpi" ] || [ "$BOX" = "bpi" ] || [ "$BOX" = "bananapi" ]
    then
        LIBS_TO_COPY+=(
            libssl
            libcrypto
        )
    fi

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
        for FILE in "$LIB_BUILD_DIR/$LIB"*.so*
        do
            if [[ $FILE != *.debug ]]
            then
                echo "Copying $(basename "$FILE")"
                copyLib "$FILE" "$LIB_INSTALL_DIR" "$ALT_LIB_INSTALL_DIR" "/$ALT_LIB_INSTALL_PATH"
            fi
        done
    done
    for LIB in "${OPTIONAL_LIBS_TO_COPY[@]}"
    do
        local FILE
        for FILE in "$LIB_BUILD_DIR/$LIB"*.so*
        do
            if [ -f "$FILE" ]
            then
                echo "Copying (optional) $(basename "$FILE")"
                copyLib "$FILE" "$LIB_INSTALL_DIR" "$ALT_LIB_INSTALL_DIR" "/$ALT_LIB_INSTALL_PATH"
            fi
        done
    done
}

# [in] LITE_CLIENT
# [in] LIB_INSTALL_DIR
# [in] ALT_LIB_INSTALL_DIR
# [in] ALT_LIB_INSTALL_PATH
copyQtLibs()
{
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
            Qml
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
        echo "Copying (Qt) $LIB_FILENAME"
        local FILE
        for FILE in "$QT_DIR/lib/$LIB_FILENAME"*
        do
            copyLib "$FILE" "$LIB_INSTALL_DIR" "$ALT_LIB_INSTALL_DIR" "/$ALT_LIB_INSTALL_PATH"
        done
    done
}

# [in] MEDIASERVER_BIN_INSTALL_DIR
copyBins()
{
    mkdir -p "$MEDIASERVER_BIN_INSTALL_DIR"
    local -r BINS_TO_COPY=(
        mediaserver
        external.dat
    )
    local BIN
    for BIN in "${BINS_TO_COPY[@]}"
    do
        local FILE
        for FILE in "$BIN_BUILD_DIR/$BIN"
        do
            echo "Copying (binary) $(basename "$FILE")"
            cp -r "$FILE" "$MEDIASERVER_BIN_INSTALL_DIR/"
        done
    done

    if [ "$BOX" = "bpi" ]
    then
        echo "Creating symlink for rpath needed by mediaserver binary"
        ln -s "../lib" "$INSTALL_DIR/mediaserver/lib"
    fi
}

# [in] MEDIASERVER_BIN_INSTALL_DIR
copyMediaserverPlugins()
{
    mkdir -p "$MEDIASERVER_BIN_INSTALL_DIR/plugins"

    if [ "$BOX" = "edge1" ]
    then
        # NOTE: Plugins from $BIN_BUILD_DIR/plugins are not needed on edge1.
        local -r PLUGIN="libcpro_ipnc_plugin.so.1.0.0"
        echo "Copying $PLUGIN"
        cp -r "$LIB_BUILD_DIR/$PLUGIN" "$MEDIASERVER_BIN_INSTALL_DIR/plugins/"
        return
    fi

    if [ -d "$BIN_BUILD_DIR/plugins" ]
    then
        local FILE
        for FILE in "$BIN_BUILD_DIR/plugins/"*
        do
            if [[ -f $FILE ]] && [[ $FILE != *.debug ]]
            then
                if [ "$ENABLE_HANWHA" != "true" ] && [[ "$FILE" == *hanwha* ]]
                then
                    continue
                fi

                echo "Copying plugins/$(basename "$FILE")"
                cp -r "$FILE" "$MEDIASERVER_BIN_INSTALL_DIR/plugins/"
            fi
        done
    fi
}

# [in] INSTALL_DIR
copyConf()
{
    if [ "$BOX" = "edge1" ]
    then
        local -r FILE="mediaserver.conf"
    else
        local -r FILE="mediaserver.conf.template"
    fi

    mkdir -p "$INSTALL_DIR/mediaserver/etc"
    echo "Copying $FILE"
    cp "$CURRENT_BUILD_DIR/opt/networkoptix/mediaserver/etc/$FILE" "$INSTALL_DIR/mediaserver/etc/"
}

# Copy the autostart script and platform-specific scripts.
# [in] TAR_DIR
# [in] INSTALL_DIR
copyScripts()
{
    if [ "$BOX" = "edge1" ]
    then
        mkdir -p "$TAR_DIR/etc/init.d"
        echo "Copying customized S99networkoptix-mediaserver"
        install -m 755 "$CURRENT_BUILD_DIR/etc/init.d/S99networkoptix-mediaserver" \
            "$TAR_DIR/etc/init.d/S99$CUSTOMIZATION-mediaserver"
    else
        echo "Copying /etc/init.d"
        cp -r "$CURRENT_BUILD_DIR/etc" "$TAR_DIR"
        chmod -R 755 "$TAR_DIR/etc/init.d"

        echo "Copying customized opt/networkoptix/*"
        cp -r "$CURRENT_BUILD_DIR/opt/networkoptix/"* "$INSTALL_DIR/"
        local -r SCRIPTS_DIR="$INSTALL_DIR/mediaserver/var/scripts"
        [ -d "$SCRIPTS_DIR" ] && chmod -R 755 "$SCRIPTS_DIR"

        NON_CUSTOMIZED_MEDIASERVER_STARTUP_SCRIPT="$TAR_DIR/etc/init.d/networkoptix-mediaserver"
        MEDIASERVER_STARTUP_SCRIPT="$TAR_DIR/etc/init.d/$CUSTOMIZATION-mediaserver"
        NON_CUSTOMIZED_LITE_CLIENT_STARTUP_SCRIPT="$TAR_DIR/etc/init.d/networkoptix-lite-client"
        LITE_CLIENT_STARTUP_SCRIPT="$TAR_DIR/etc/init.d/$CUSTOMIZATION-lite-client"

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

# [in] TAR_DIR
copyDebs()
{
    local -r DEBS_DIR="$BUILD_DIR/deb"
    if [ -d "$DEBS_DIR" ]
    then
        echo "Copying .deb packages"
        cp -r "$DEBS_DIR" "$TAR_DIR/opt/"
    fi
}

# [in] INSTALL_DIR
# [in] LIB_INSTALL_DIR
copyBpiLiteClient()
{
    local -r LITE_CLIENT_BIN_DIR="$INSTALL_DIR/lite_client/bin"

    if [ -d "$LIB_BUILD_DIR/ffmpeg" ]
    then
        echo "Copying libs of a dedicated ffmpeg for Lite Client's proxydecoder"
        cp -r "$LIB_BUILD_DIR/ffmpeg" "$LIB_INSTALL_DIR/"
    fi

    echo "Copying mobile_client binary"
    mkdir -p "$LITE_CLIENT_BIN_DIR"
    cp "$BIN_BUILD_DIR/mobile_client" "$LITE_CLIENT_BIN_DIR/"

    echo "Creating symlink for rpath needed by mobile_client binary"
    ln -s "../lib" "$INSTALL_DIR/lite_client/lib"

    echo "Creating symlink for rpath needed by Qt plugins"
    ln -s "../../lib" "$LITE_CLIENT_BIN_DIR/lib"

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
        cp -r "$BIN_BUILD_DIR/$DIR" "$LITE_CLIENT_BIN_DIR/"
    done

    echo "Copying Qt translations"
    cp -r "$QT_DIR/translations" "$LITE_CLIENT_BIN_DIR/"

    # TODO: Investigate how to get rid of "resources" duplication.
    echo "Copying Qt resources"
    cp -r "$QT_DIR/resources" "$LITE_CLIENT_BIN_DIR/"
    cp -r "$QT_DIR/resources/"* "$LITE_CLIENT_BIN_DIR/libexec/"

    echo "Copying qt.conf"
    cp -r "$SOURCE_DIR/common/maven/bin-resources/resources/qt/etc/qt.conf" "$LITE_CLIENT_BIN_DIR/"
}

# [in] TAR_DIR
copyBpiSpecificFiles()
{
    if [ -d "$BUILD_DIR/root" ]
    then
        echo "Copying (bpi) uboot files (linux kernel upgrade) to root/"
        cp -r "$BUILD_DIR/root" "$TAR_DIR/"
    fi

    if [ -d "$BUILD_DIR/usr" ]
    then
        echo "Copying (bpi) usr/"
        cp -r "$BUILD_DIR/usr" "$TAR_DIR/"
    fi

    echo "Copying (bpi) root/"
    cp -r "$CURRENT_BUILD_DIR/root" "$TAR_DIR/"

    local -r TOOLS_PATH="root/tools/$CUSTOMIZATION"
    local -r TOOLS_DIR="$TAR_DIR/$TOOLS_PATH"
    local -r CONF_FILE="mediaserver.conf.template"
    echo "Copying $CONF_FILE (used for factory reset) to $TOOLS_PATH/"
    mkdir -p "$TOOLS_DIR"
    cp "$CURRENT_BUILD_DIR/opt/networkoptix/mediaserver/etc/$CONF_FILE" "$TOOLS_DIR/"
}

copyEdge1SpecificFiles()
{
    local -r GDB_DIR="$INSTALL_DIR/mediaserver/bin"
    echo "Copying gdb to $GDB_DIR/"
    cp -r "$PACKAGES_DIR/gdb"/* "$GDB_DIR/"
}

# [in] INSTALL_DIR
# [in] LIB_INSTALL_DIR
copyAdditionalSysrootFilesIfNeeded()
{
    local -r SYSROOT_LIB_DIR="$SYSROOT_DIR/usr/lib/arm-linux-gnueabihf"
    local -r SYSROOT_BIN_DIR="$SYSROOT_DIR/usr/bin"

    if [ "$BOX" = "bpi" ]
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
            cp -r "$SYSROOT_LIB_DIR/$LIB"* "$LIB_INSTALL_DIR/"
        done
    elif [ "$BOX" = "bananapi" ]
    then
        echo "Copying (sysroot) libglib required for bananapi on Debian 8 \"Jessie\""
        cp -r "$SYSROOT_LIB_DIR/libglib"* "$LIB_INSTALL_DIR/"
        echo "Copying (sysroot) hdparm required for bananapi on Debian 8 \"Jessie\""
        cp -r "$SYSROOT_BIN_DIR/hdparm" "$INSTALL_DIR/mediaserver/bin/"
    elif [ "$BOX" = "rpi" ]
    then
        echo "Copying (sysroot) hdparm"
        cp -r "$SYSROOT_BIN_DIR/hdparm" "$INSTALL_DIR/mediaserver/bin/"
    fi
}

# [in] INSTALL_DIR
# [in] VOX_SOURCE_DIR
copyVox()
{
    if [ -d "$VOX_SOURCE_DIR" ]
    then
        local -r VOX_INSTALL_DIR="$INSTALL_DIR/mediaserver/bin/vox"
        echo "Copying Festival VOX files"
        mkdir -p "$VOX_INSTALL_DIR"
        cp -r "$VOX_SOURCE_DIR/"* "$VOX_INSTALL_DIR/"
    fi
}

# [in] LIB_INSTALL_DIR
copyToolchainLibs()
{
    echo "Copying toolchain libs (libstdc++, libatomic)"
    copy_sys_lib "libstdc++.so.6" "$LIB_INSTALL_DIR"
    copy_sys_lib "libgcc_s.so.1" "$LIB_INSTALL_DIR"
    copy_sys_lib "libatomic.so.1" "$LIB_INSTALL_DIR"
}

# [in] WORK_DIR
# [in] TAR_DIR
# [in] OUTPUT_DIR
buildInstaller()
{
    echo ""
    echo "Creating distribution .tar.gz"
    if [ ! -z "$SYMLINK_INSTALL_PATH" ]
    then
        mkdir -p "$TAR_DIR/$(dirname "$SYMLINK_INSTALL_PATH")"
        ln -s "/$INSTALL_PATH" "$TAR_DIR/$SYMLINK_INSTALL_PATH"
    fi
    createArchive "$OUTPUT_DIR/$TAR_FILENAME" "$TAR_DIR" tar czf

    echo ""
    echo "Creating \"update\" .zip"
    local -r ZIP_DIR="$WORK_DIR/zip"
    mkdir -p "$ZIP_DIR"
    cp -r "$OUTPUT_DIR/$TAR_FILENAME" "$ZIP_DIR/"
    cp -r "$CURRENT_BUILD_DIR"/update.* "$ZIP_DIR/"
    cp -r "$CURRENT_BUILD_DIR"/install.sh "$ZIP_DIR/"
    createArchive "$OUTPUT_DIR/$ZIP_FILENAME" "$ZIP_DIR" zip
    cp "$OUTPUT_DIR/$ZIP_FILENAME" "$OUTPUT_DIR/$ZIP_INSTALLER_FILENAME"
}

# [in] WORK_DIR
# [in] OUTPUT_DIR
buildDebugSymbolsArchive()
{
    echo ""
    echo "Creating debug symbols .tar.gz"

    local -r DEBUG_TAR_DIR="$WORK_DIR/debug-symbols-tar"
    mkdir -p "$DEBUG_TAR_DIR"

    local -i DEBUG_FILES_EXIST=0
    local FILE
    for FILE in \
        "$LIB_BUILD_DIR"/*.debug \
        "$BIN_BUILD_DIR"/*.debug \
        "$BIN_BUILD_DIR/plugins"/*.debug \
        "$BIN_BUILD_DIR/plugins_optional"/*.debug
    do
        if [ -f "$FILE" ]
        then
            DEBUG_FILES_EXIST=1
            echo "  Copying $(basename $FILE)"
            cp -r "$FILE" "$DEBUG_TAR_DIR/"
        fi
    done

    if [ $DEBUG_FILES_EXIST = 0 ]
    then
        echo "  No .debug files found"
    else
        createArchive \
            "$OUTPUT_DIR/$TAR_FILENAME-debug-symbols.tar.gz" "$DEBUG_TAR_DIR" tar czf
    fi
}

#--------------------------------------------------------------------------------------------------

# [in] LITE_CLIENT
# [in] WORK_DIR
buildDistribution()
{
    local -r TAR_DIR="$WORK_DIR/tar"
    local -r INSTALL_DIR="$TAR_DIR/$INSTALL_PATH"
    local -r LIB_INSTALL_DIR="$TAR_DIR/$LIB_INSTALL_PATH"
    local -r MEDIASERVER_BIN_INSTALL_DIR="$INSTALL_DIR/mediaserver/bin"

    mkdir -p "$INSTALL_DIR"
    mkdir -p "$LIB_INSTALL_DIR"
    if [ -z "$ALT_LIB_INSTALL_PATH" ]
    then
        local -r ALT_LIB_INSTALL_DIR=""
    else
        local -r ALT_LIB_INSTALL_DIR="$TAR_DIR/$ALT_LIB_INSTALL_PATH"
        mkdir -p "$ALT_LIB_INSTALL_DIR"
    fi

    echo "Generating version.txt: $VERSION"
    echo "$VERSION" >"$INSTALL_DIR/version.txt"

    echo "Copying build_info.txt"
    cp -r "$BUILD_DIR/build_info.txt" "$INSTALL_DIR/"

    copyBuildLibs
    copyQtLibs
    copyBins
    copyMediaserverPlugins
    copyConf
    copyScripts
    copyDebs
    copyAdditionalSysrootFilesIfNeeded
    copyVox
    copyToolchainLibs

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

    buildInstaller
    buildDebugSymbolsArchive
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
    local -i LITE_CLIENT=1
    distr_EXTRA_HELP=" --no-client: Do not pack Lite Client."
    distr_PARSE_EXTRA_ARGS_FUNC=parseExtraArgs

    distr_prepareToBuildDistribution "$WORK_DIR" "$LOG_FILE" "$@"

    buildDistribution
}

main "$@"
