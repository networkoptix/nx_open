#!/bin/bash
set -e

# ATTENTION: This script works with both maven and cmake builds.

COMPANY_NAME=@deb.customization.company.name@
FULL_COMPANY_NAME="@company.name@"
FULL_PRODUCT_NAME="@company.name@ @product.name@ Client.conf"
FULL_APPLAUNCHER_NAME="@company.name@ Launcher.conf"

VERSION=@release.version@
FULLVERSION=@release.version@.@buildNumber@
MINORVERSION=@parsedVersion.majorVersion@.@parsedVersion.minorVersion@
ARCHITECTURE=@os.arch@

COMPILER=@CMAKE_CXX_COMPILER@
SOURCE_ROOT_PATH="@root.dir@"
TARGET=/opt/$COMPANY_NAME/client/$FULLVERSION
USRTARGET=/usr
BINTARGET=$TARGET/bin
BGTARGET=$TARGET/share/pictures/sample-backgrounds
HELPTARGET=$TARGET/help
ICONTARGET=$USRTARGET/share/icons
LIBTARGET=$TARGET/lib
INITTARGET=/etc/init
INITDTARGET=/etc/init.d

FINALNAME=@artifact.name.client@
UPDATE_NAME=@artifact.name.client_update@.zip

STAGEBASE=stagebase
STAGE=$STAGEBASE/$FINALNAME
STAGETARGET=$STAGE/$TARGET
BINSTAGE=$STAGE$BINTARGET
BGSTAGE=$STAGE$BGTARGET
HELPSTAGE=$STAGE$HELPTARGET
ICONSTAGE=$STAGE$ICONTARGET
LIBSTAGE=$STAGE$LIBTARGET

CLIENT_BIN_PATH=@libdir@/bin/@build.configuration@
CLIENT_IMAGEFORMATS_PATH=$CLIENT_BIN_PATH/imageformats
CLIENT_MEDIASERVICE_PATH=$CLIENT_BIN_PATH/mediaservice
CLIENT_AUDIO_PATH=$CLIENT_BIN_PATH/audio
CLIENT_XCBGLINTEGRATIONS_PATH=$CLIENT_BIN_PATH/xcbglintegrations
CLIENT_PLATFORMINPUTCONTEXTS_PATH=$CLIENT_BIN_PATH/platforminputcontexts
CLIENT_QML_PATH=$CLIENT_BIN_PATH/qml
CLIENT_VOX_PATH=$CLIENT_BIN_PATH/vox
CLIENT_PLATFORMS_PATH=$CLIENT_BIN_PATH/platforms
CLIENT_BG_PATH=@libdir@/backgrounds
CLIENT_HELP_PATH=@ClientHelpSourceDir@
ICONS_PATH=@customization.dir@/icons/linux/hicolor
CLIENT_LIB_PATH=@libdir@/lib/@build.configuration@
BUILD_INFO_TXT=@libdir@/build_info.txt
LOGS_DIR="@libdir@/build_logs"
LOG_FILE="$LOGS_DIR/client-build-dist.log"

cp_sys_lib()
{
    "$SOURCE_ROOT_PATH"/build_utils/copy_system_library -c "$COMPILER" "$@"
}

buildDistribution()
{
    echo "Creating directories"
    mkdir -p $BINSTAGE/imageformats
    mkdir -p $BINSTAGE/mediaservice
    mkdir -p $BINSTAGE/audio
    mkdir -p $BINSTAGE/platforminputcontexts
    mkdir -p $HELPSTAGE
    mkdir -p $LIBSTAGE
    mkdir -p $BGSTAGE
    mkdir -p $ICONSTAGE
    mkdir -p "$STAGE/etc/xdg/$FULL_COMPANY_NAME"

    echo "Copying client.conf and applauncher.conf"
    cp debian/client.conf $STAGE/etc/xdg/"$FULL_COMPANY_NAME"/"$FULL_PRODUCT_NAME"
    cp debian/applauncher.conf $STAGE/etc/xdg/"$FULL_COMPANY_NAME"/"$FULL_APPLAUNCHER_NAME"

    echo "Copying build_info.txt"
    cp -r $BUILD_INFO_TXT $BINSTAGE/../

    echo "Copying qt.conf"
    cp -r qt.conf $BINSTAGE/

    echo "Copying client binaries and old version libs"
    cp -r $CLIENT_BIN_PATH/desktop_client $BINSTAGE/client-bin
    cp -r $CLIENT_BIN_PATH/applauncher $BINSTAGE/applauncher-bin
    cp -r bin/client $BINSTAGE
    cp -r $CLIENT_BIN_PATH/@launcher.version.file@ $BINSTAGE
    cp -r bin/applauncher $BINSTAGE

    echo "Copying icons"
    cp -P -Rf usr $STAGE
    mv -f "$STAGE/usr/share/applications/icon.desktop" "$STAGE/usr/share/applications/@installer.name@.desktop"
    mv -f "$STAGE/usr/share/applications/protocol.desktop" "$STAGE/usr/share/applications/@uri.protocol@.desktop"
    cp -P -Rf $ICONS_PATH $ICONSTAGE
    for f in $(find $ICONSTAGE -name "*.png")
    do
        mv $f `dirname $f`/`basename $f .png`-@customization@.png
    done

    echo "Copying help"
    cp -r $CLIENT_HELP_PATH/* $HELPSTAGE

    echo "Copying fonts"
    cp -r "$CLIENT_BIN_PATH/fonts" "$BINSTAGE"

    echo "Copying backgrounds"
    cp -r $CLIENT_BG_PATH/* $BGSTAGE

    local LIB
    local LIB_BASENAME
    for LIB in "$CLIENT_LIB_PATH"/*.so*
    do
        LIB_BASENAME=$(basename "$LIB")
        if [[ "$LIB_BASENAME" != libQt5* \
            && "$LIB_BASENAME" != libEnginio.so* \
            && "$LIB_BASENAME" !=  libqgsttools_p.* ]]
        then
            echo "Copying $LIB_BASENAME"
            cp -P "$LIB" "$LIBSTAGE/"
        fi
    done

    echo "Copying (Qt plugin) platforminputcontexts"
    cp -r $CLIENT_PLATFORMINPUTCONTEXTS_PATH/*.* $BINSTAGE/platforminputcontexts
    echo "Copying (Qt plugin) imageformats"
    cp -r $CLIENT_IMAGEFORMATS_PATH/*.* $BINSTAGE/imageformats
    echo "Copying (Qt plugin) mediaservice"
    cp -r $CLIENT_MEDIASERVICE_PATH/*.* $BINSTAGE/mediaservice
    echo "Copying (Qt plugin) xcbglintegrations"
    cp -r $CLIENT_XCBGLINTEGRATIONS_PATH $BINSTAGE/
    echo "Copying (Qt plugin) qml"
    cp -r $CLIENT_QML_PATH $BINSTAGE
    if [ '@arch@' != 'arm' ]
    then
        echo "Copying (Qt plugin) audio"
        cp -r $CLIENT_AUDIO_PATH/*.* $BINSTAGE/audio
    fi
    echo "Copying (Qt plugin) platforms"
    cp -r $CLIENT_PLATFORMS_PATH $BINSTAGE/

    echo "Copying Festival VOX files"
    cp -r $CLIENT_VOX_PATH $BINSTAGE

    echo "Deleting .debug libs (if any)"
    rm -f $LIBSTAGE/*.debug

    QTLIBS="Core Gui Widgets WebKit WebChannel WebKitWidgets OpenGL Multimedia MultimediaQuick_p Qml Quick QuickWidgets LabsTemplates X11Extras XcbQpa DBus Xml XmlPatterns Concurrent Network Sql PrintSupport"
    if [ '@arch@' == 'arm' ]
    then
        QTLIBS+=( Sensors )
    fi
    for var in $QTLIBS
    do
        qtlib=libQt5$var.so
        echo "Copying (Qt) $qtlib"
        cp -P @qt.dir@/lib/$qtlib* $LIBSTAGE
    done

    cp_sys_lib libstdc++.so.6 "$LIBSTAGE"

    if [ '@arch@' != 'arm' ]
    then
        echo "Copying additional libs"
        cp_sys_lib libXss.so.1 "$LIBSTAGE"
        cp_sys_lib libpng12.so.0 "$LIBSTAGE" || cp_sys_lib libpng.so "$LIBSTAGE"
        cp_sys_lib libopenal.so.1 "$LIBSTAGE"
        cp -P @qt.dir@/lib/libicu*.so* "$LIBSTAGE"
    fi

    echo "Setting permissions"
    find $PKGSTAGE -type d -print0 | xargs -0 chmod 755
    find $PKGSTAGE -type f -print0 | xargs -0 chmod 644
    chmod 755 $BINSTAGE/*

    echo "Preparing DEBIAN dir"
    mkdir -p $STAGE/DEBIAN
    chmod g-s,go+rx $STAGE/DEBIAN

    INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

    echo "Generating DEBIAN/control"
    cat debian/control.template \
        | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" \
        | sed "s/VERSION/$VERSION/g" \
        | sed "s/ARCHITECTURE/$ARCHITECTURE/g" \
        > $STAGE/DEBIAN/control

    echo "Copying DEBIAN dir"
    for f in $(ls debian)
    do
        if [ $f != 'control.template' ]
        then
            install -m 755 debian/$f $STAGE/DEBIAN
        fi
    done

    echo "Generating DEBIAN/md5sums"
    (cd $STAGE
        find * -type f -not -regex '^DEBIAN/.*' -print0 | xargs -0 md5sum > DEBIAN/md5sums
        chmod 644 DEBIAN/md5sums
    )

    echo "Creating .deb $FINALNAME.deb"
    (cd $STAGEBASE
        fakeroot dpkg-deb -b $FINALNAME
    )

    echo "Copying scalable/apps icons"
    mkdir -p "$STAGETARGET/share/icons"
    cp "$ICONSTAGE/hicolor/scalable/apps"/* "$STAGETARGET/share/icons"

    echo "Copying update.json"
    cp "bin/update.json" "$STAGETARGET"

    echo "Generating finalname-client.properties"
    echo "client.finalName=$FINALNAME" >> finalname-client.properties

    echo "Creating .zip $UPDATE_NAME"
    (cd "$STAGETARGET"; zip -y -r "$UPDATE_NAME" ./*)

    if [ '@distribution_output_dir@' != '' ]
    then
        echo "Moving distribution .zip and .deb to @distribution_output_dir@"
        mv "$STAGETARGET/$UPDATE_NAME" "@distribution_output_dir@/"
        mv "$STAGEBASE/$FINALNAME.deb" "@distribution_output_dir@/"
    else
        echo "Moving distribution .zip to @project.build.directory@"
        mv "$STAGETARGET/$UPDATE_NAME" "@project.build.directory@/"
    fi
}

showHelp()
{
    echo "Options:"
    echo " -v, --verbose: Do not redirect output to a log file."
    echo " -k, --keep-work-dir: Do not delete work directory on success."
}

# [out] KEEP_WORK_DIR
# [out] VERBOSE
parseArgs() # "$@"
{
    KEEP_WORK_DIR=0
    VERBOSE=0

    local ARG
    for ARG in "$@"; do
        if [ "$ARG" = "-h" -o "$ARG" = "--help" ]; then
            showHelp
            exit 0
        elif [ "$ARG" = "-k" ] || [ "$ARG" = "--keep-work-dir" ]; then
            KEEP_WORK_DIR=1
        elif [ "$ARG" = "-v" ] || [ "$ARG" = "--verbose" ]; then
            VERBOSE=1
        fi
    done
}

redirectOutputToLog()
{
    echo "  See the log in $LOG_FILE"

    mkdir -p "$LOGS_DIR" #< In case log dir is not created yet.
    rm -rf "$LOG_FILE"

    # Redirect only stdout to the log file, leaving stderr as is, because there seems to be no
    # reliable way to duplicate error output to both log file and stderr, keeping the correct order
    # of regular and error lines in the log file.
    exec 1>"$LOG_FILE" #< Open log file for writing as stdout.
}

# Called by trap.
# [in] $?
# [in] VERBOSE
onExit()
{
    local RESULT=$?
    echo "" #< Newline, to separate SUCCESS/FAILURE message from the above log.
    if [ $RESULT != 0 ]; then #< Failure.
        echo "FAILURE (status $RESULT); see the error message(s) above." >&2 #< To stderr.
        if [ $VERBOSE = 0 ]; then #< stdout redirected to log file.
            echo "FAILURE (status $RESULT); see the error message(s) on stderr." #< To log file.
        fi
    else
        echo "SUCCESS"
    fi
    return $RESULT
}

main()
{
    declare -i VERBOSE #< Mot local - needed by onExit().
    parseArgs "$@"

    trap onExit EXIT

    rm -rf "$STAGEBASE"

    if [ KEEP_WORK_DIR = 1 ]; then
        local -r WORK_DIR_NOTE="(ATTENTION: will NOT be deleted)"
    else
        local -r WORK_DIR_NOTE="(will be deleted on success)"
    fi
    echo "Creating distribution in $STAGEBASE $WORK_DIR_NOTE."

    if [ $VERBOSE = 0 ]; then
        redirectOutputToLog
    fi

    buildDistribution

    if [ KEEP_WORK_DIR = 0 ]; then
        rm -rf "$STAGEBASE"
    fi
}

main "$@"
