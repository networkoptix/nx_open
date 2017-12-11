#!/bin/bash
set -e #< Exit on any error.
set -u #< Prohibit undefined variables.

# ATTENTION: This script works with both maven and cmake builds.

: ${CONFIG="create_arm_installer.conf"} #< CONFIG can be overridden externally.
if [ ! -z "$CONFIG" ]; then
    source "$CONFIG"
fi

if [ ! -z "$BUILD_CONFIGURATION" ]; then
    LIB_AND_BIN_DIR_SUFFIX="/$BUILD_CONFIGURATION"
else
    LIB_AND_BIN_DIR_SUFFIX=""
fi
LIB_BUILD_DIR="$BUILD_DIR/lib$LIB_AND_BIN_DIR_SUFFIX"
BIN_BUILD_DIR="$BUILD_DIR/bin$LIB_AND_BIN_DIR_SUFFIX"

# VMS files will be copied to this path.
INSTALL_PATH="opt/$CUSTOMIZATION"

# If not empty, a symlink will be created from this path to $INSTALL_PATH.
SYMLINK_INSTALL_PATH=""

# To save storage space on the device, some .so files can be copied to an alternative location
# defined by ALT_LIB_INSTALL_PATH (e.g. an sdcard which does not support symlinks), and symlinks
# to these .so files will be created in the regular LIB_INSTALL_DIR.
ALT_LIB_INSTALL_PATH=""

OUTPUT_DIR=${DISTRIB_OUTPUT_DIR:-"$CURRENT_BUILD_DIR"}

if [ "$BOX" = "edge1" ]; then
    INSTALL_PATH="usr/local/apps/$CUSTOMIZATION"
    SYMLINK_INSTALL_PATH="opt/$CUSTOMIZATION"
    ALT_LIB_INSTALL_PATH="sdcard/${CUSTOMIZATION}_service"
fi

if [ "$BOX" = "bpi" ]; then
    LIB_INSTALL_PATH="$INSTALL_PATH/lib"
else
    LIB_INSTALL_PATH="$INSTALL_PATH/mediaserver/lib"
fi

#--------------------------------------------------------------------------------------------------

help()
{
    echo "Options:"
    echo " --no-client: Do not pack Lite Client."
    echo " -v, --verbose: Do not redirect output to a log file."
}

# [out] LITE_CLIENT
# [out] VERBOSE
parseArgs() # "$@"
{
    LITE_CLIENT=1
    VERBOSE=0

    local ARG
    for ARG in "$@"; do
        if [ "$ARG" = "-h" -o "$ARG" = "--help" ]; then
            help
            exit 0
        elif [ "$ARG" = "--no-client" ] ; then
            LITE_CLIENT=0
        elif [ "$ARG" = "-v" ] || [ "$ARG" = "--verbose" ]; then
            VERBOSE=1
        fi
    done
}

redirectOutput() # log-file
{
    local -r LOG_FILE="$1"; shift

    echo "  See the log in $LOG_FILE"

    rm -rf "$LOG_FILE"
    exec 1<&- #< Close stdout fd.
    exec 2<&- #< Close stderr fd.
    exec 1<>"$LOG_FILE" #< Open stdout as $LOG_FILE for reading and writing.
    exec 2>&1 #< Redirect stderr to stdout.
}

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

    if [ ! -z "$ALT_LIB_DIR" ] && [ ! -L "$FILE" ]; then
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
        libnx_email
        libnx_fusion
        libnx_kit
        libnx_network
        libnx_speech_synthesizer
        libnx_utils

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
        liblber
        libldap
        libquazip
        libsasl2
        libsigar
        libudt
    )

    local OPTIONAL_LIBS_TO_COPY=(
        libvpx
    )

    # Libs for BananaPi-based platforms.
    if [ "$BOX" = "bpi" ] || [ "$BOX" = "bananapi" ]; then
        LIBS_TO_COPY+=(
            libGLESv2
            libMali
            libUMP
        )
    fi

    # Libs for lite client.
    if [ "$BOX" = "bpi" ] && [ $LITE_CLIENT = 1 ]; then
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

    local LIB
    for LIB in "${LIBS_TO_COPY[@]}"; do
        local FILE
        for FILE in "$LIB_BUILD_DIR/$LIB"*.so*; do
            if [[ $FILE != *.debug ]]; then
                echo "Copying $(basename "$FILE")"
                copyLib "$FILE" "$LIB_INSTALL_DIR" "$ALT_LIB_INSTALL_DIR" "/$ALT_LIB_INSTALL_PATH"
            fi
        done
    done
    for LIB in "${OPTIONAL_LIBS_TO_COPY[@]}"; do
        local FILE
        for FILE in "$LIB_BUILD_DIR/$LIB"*.so*; do
            if [ -f "$FILE" ]; then
                echo "Copying (optional) $(basename "$FILE")"
                copyLib "$FILE" "$LIB_INSTALL_DIR" "$ALT_LIB_INSTALL_DIR" "/$ALT_LIB_INSTALL_PATH"
            fi
        done
    done
}

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
    if [ "$BOX" = "bpi" ] && [ $LITE_CLIENT = 1 ]; then
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
    for QT_LIB in "${QT_LIBS_TO_COPY[@]}"; do
        local LIB_FILENAME="libQt5$QT_LIB.so"
        echo "Copying (Qt) $LIB_FILENAME"
        local FILE
        for FILE in "$QT_DIR/lib/$LIB_FILENAME"*; do
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
    for BIN in "${BINS_TO_COPY[@]}"; do
        local FILE
        for FILE in "$BIN_BUILD_DIR/$BIN"; do
            echo "Copying (binary) $(basename "$FILE")"
            cp -r "$FILE" "$MEDIASERVER_BIN_INSTALL_DIR/"
        done
    done

    mkdir -p "$MEDIASERVER_BIN_INSTALL_DIR/plugins"
    if [ "$BOX" = "edge1" ]; then
        # NOTE: Plugins from $BIN_BUILD_DIR/plugins are not needed on edge1.
        local -r PLUGIN="libcpro_ipnc_plugin.so.1.0.0"
        echo "Copying $PLUGIN"
        cp -r "$LIB_BUILD_DIR/$PLUGIN" "$MEDIASERVER_BIN_INSTALL_DIR/plugins/"
    else
        if [ -d "$BIN_BUILD_DIR/plugins" ]; then
            local FILE
            for FILE in "$BIN_BUILD_DIR/plugins/"*; do
                if [[ $FILE != *.debug ]]; then
                    if [ "$CUSTOMIZATION" != "hanwha" ] && [[ "$FILE" == *hanwha* ]]; then
                        continue
                    fi

                    echo "Copying plugins/$(basename "$FILE")"
                    cp -r "$FILE" "$MEDIASERVER_BIN_INSTALL_DIR/plugins/"
                fi
            done
        fi
    fi
}

# [in] INSTALL_DIR
copyConf()
{
    if [ "$BOX" = "edge1" ]; then
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
    if [ "$BOX" = "edge1" ]; then
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

        if [ ! "$CUSTOMIZATION" = "networkoptix" ]; then
            if [ -f "$NON_CUSTOMIZED_MEDIASERVER_STARTUP_SCRIPT" ]; then
                mv "$NON_CUSTOMIZED_MEDIASERVER_STARTUP_SCRIPT" "$MEDIASERVER_STARTUP_SCRIPT"
            fi
            if [ -f "$NON_CUSTOMIZED_LITE_CLIENT_STARTUP_SCRIPT" ]; then
                mv "$NON_CUSTOMIZED_LITE_CLIENT_STARTUP_SCRIPT" "$LITE_CLIENT_STARTUP_SCRIPT"
            fi
        fi
    fi
}

# [in] TAR_DIR
copyDebs()
{
    local -r DEBS_DIR="$BUILD_DIR/deb"
    if [ -d "$DEBS_DIR" ]; then
        echo "Copying .deb packages"
        cp -r "$DEBS_DIR" "$TAR_DIR/opt/"
    fi
}

# [in] INSTALL_DIR
# [in] LIB_INSTALL_DIR
copyBpiLiteClient()
{
    local -r LITE_CLIENT_BIN_DIR="$INSTALL_DIR/lite_client/bin"

    if [ -d "$LIB_BUILD_DIR/ffmpeg" ]; then
        echo "Copying libs of a dedicated ffmpeg for Lite Client's proxydecoder"
        cp -r "$LIB_BUILD_DIR/ffmpeg" "$LIB_INSTALL_DIR/"
    fi

    echo "Copying lite_client bin"
    mkdir -p "$LITE_CLIENT_BIN_DIR"
    cp "$BIN_BUILD_DIR/mobile_client" "$LITE_CLIENT_BIN_DIR/"

    echo "Creating symlink for rpath needed by mediaserver binary"
    ln -s "../lib" "$INSTALL_DIR/mediaserver/lib"

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
    for DIR in "${DIRS_TO_COPY[@]}"; do
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
    echo "Copying (bpi) uboot files to root/"
    cp -r "$BUILD_DIR/root" "$TAR_DIR/"

    echo "Copying (bpi) usr/"
    cp -r "$BUILD_DIR/usr" "$TAR_DIR/"

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
    local -r SYSROOT_LIB_DIR="$PACKAGES_DIR/sysroot/usr/lib/arm-linux-gnueabihf"

    if [ "$BOX" = "bpi" ]; then
        local SYSROOT_LIBS_TO_COPY=(
            libopus
            libvpx
            libwebpdemux
            libwebp
        )
        local LIB
        for LIB in "${SYSROOT_LIBS_TO_COPY[@]}"; do
            echo "Copying (sysroot) $LIB"
            cp -r "$SYSROOT_LIB_DIR/$LIB"* "$LIB_INSTALL_DIR/"
        done
    elif [ "$BOX" = "bananapi" ]; then
        echo "Copying (sysroot) libglib required for bananapi on Debian 8 \"Jessie\""
        cp -r "$SYSROOT_LIB_DIR/libglib"* "$LIB_INSTALL_DIR/"
        echo "Copying (sysroot) hdparm required for bananapi on Debian 8 \"Jessie\""
        cp -r "$PACKAGES_DIR/sysroot/usr/bin/hdparm" "$INSTALL_DIR/mediaserver/bin/"
    fi
}

# [in] INSTALL_DIR
# [in] VOX_SOURCE_DIR
copyVox()
{
    if [ -d "$VOX_SOURCE_DIR" ]; then
        local -r VOX_INSTALL_DIR="$INSTALL_DIR/mediaserver/bin/vox"
        echo "Copying Festival VOX files"
        mkdir -p "$VOX_INSTALL_DIR"
        cp -r "$VOX_SOURCE_DIR/"* "$VOX_INSTALL_DIR/"
    fi
}

# [in] LIB_INSTALL_DIR
copyLibcIfNeeded()
{
    # TODO: Consider unconditionally copying from the compiler artifact. Decision on usage will
    # then be made in install.sh on the box.
    if [ "$BOX" = "bpi" ] || [ "$BOX" = "bananapi" ]; then
        echo "Copying libstdc++ (Banana Pi)"
        cp -r "$PACKAGES_DIR/libstdc++-6.0.19/lib/libstdc++.so"* "$LIB_INSTALL_DIR/"
    fi
}

# [in] WORK_DIR
# [in] TAR_DIR
# [in] OUTPUT_DIR
buildInstaller()
{
    echo ""
    echo "Creating installer .tar.gz"
    if [ ! -z "$SYMLINK_INSTALL_PATH" ]; then
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
    ; do
        if [ -f "$FILE" ]; then
            DEBUG_FILES_EXIST=1
            echo "  Copying $(basename $FILE)"
            cp -r "$FILE" "$DEBUG_TAR_DIR/"
        fi
    done

    if [ $DEBUG_FILES_EXIST = 0 ]; then
        echo "  No .debug files found"
    else
        createArchive \
            "$OUTPUT_DIR/$TAR_FILENAME-debug-symbols.tar.gz" "$DEBUG_TAR_DIR" tar czf
    fi
}

#--------------------------------------------------------------------------------------------------

main()
{
    local -i LITE_CLIENT
    local -i VERBOSE
    parseArgs "$@"

    local -r WORK_DIR="$BUILD_DIR/arm_installer"
    rm -rf "$WORK_DIR"

    local -r TAR_DIR="$WORK_DIR/tar"
    local -r INSTALL_DIR="$TAR_DIR/$INSTALL_PATH"
    local -r LIB_INSTALL_DIR="$TAR_DIR/$LIB_INSTALL_PATH"
    local -r MEDIASERVER_BIN_INSTALL_DIR="$INSTALL_DIR/mediaserver/bin"

    echo "Creating installer in $WORK_DIR (will be deleted on success)."

    if [ $VERBOSE = 0 ]; then
        redirectOutput "$BUILD_DIR/create_arm_installer.log"
    fi

    mkdir -p "$INSTALL_DIR"
    mkdir -p "$LIB_INSTALL_DIR"
    if [ -z "$ALT_LIB_INSTALL_PATH" ]; then
        local -r ALT_LIB_INSTALL_DIR=""
    else
        local -r ALT_LIB_INSTALL_DIR="$TAR_DIR/$ALT_LIB_INSTALL_PATH"
        mkdir -p "$ALT_LIB_INSTALL_DIR"
    fi

    echo "Creating version.txt: $VERSION"
    echo "$VERSION" >"$INSTALL_DIR/version.txt"

    echo "Copying build_info.txt"
    cp -r "$BUILD_DIR/build_info.txt" "$INSTALL_DIR/"

    copyBuildLibs
    copyQtLibs
    copyBins
    copyConf
    copyScripts
    copyDebs
    copyAdditionalSysrootFilesIfNeeded
    copyVox
    copyLibcIfNeeded

    if [ "$BOX" = "bpi" ]; then
        copyBpiSpecificFiles
        if [ $LITE_CLIENT = 1 ]; then
            copyBpiLiteClient
        fi
    elif [ "$BOX" = "edge1" ]; then
        copyEdge1SpecificFiles
    fi

    buildInstaller
    buildDebugSymbolsArchive

    rm -rf "$WORK_DIR"

    echo ""
    echo "FINISHED"
}

main "$@"
