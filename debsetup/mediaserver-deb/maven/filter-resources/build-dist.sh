#!/bin/bash
set -e

# ATTENTION: This script works with both maven and cmake builds.

COMPANY_NAME=@deb.customization.company.name@

VERSION=@release.version@
ARCHITECTURE=@os.arch@

COMPILER=@CMAKE_CXX_COMPILER@
SOURCE_ROOT_PATH=@root.dir@
TARGET=/opt/$COMPANY_NAME/mediaserver
BINTARGET=$TARGET/bin
LIBTARGET=$TARGET/lib
LIBPLUGINTARGET=$BINTARGET/plugins
SHARETARGET=$TARGET/share
ETCTARGET=$TARGET/etc
INITTARGET=/etc/init
INITDTARGET=/etc/init.d
SYSTEMDTARGET=/etc/systemd/system

FINALNAME=@artifact.name.server@
UPDATE_NAME=@artifact.name.server_update@.zip

STAGEBASE=stagebase
STAGE=$STAGEBASE/$FINALNAME
BINSTAGE=$STAGE$BINTARGET
LIBSTAGE=$STAGE$LIBTARGET
LIBPLUGINSTAGE=$STAGE$LIBPLUGINTARGET
SHARESTAGE=$STAGE$SHARETARGET
ETCSTAGE=$STAGE$ETCTARGET
INITSTAGE=$STAGE$INITTARGET
INITDSTAGE=$STAGE$INITDTARGET
SYSTEMDSTAGE=$STAGE$SYSTEMDTARGET

SERVER_BIN_PATH=@libdir@/bin/@build.configuration@
SERVER_SHARE_PATH=@libdir@/share
SERVER_DEB_PATH=@libdir@/deb
SERVER_VOX_PATH=$SERVER_BIN_PATH/vox
SERVER_LIB_PATH=@libdir@/lib/@build.configuration@
SERVER_LIB_PLUGIN_PATH=$SERVER_BIN_PATH/plugins
SCRIPTS_PATH=@basedir@/../scripts
BUILD_INFO_TXT=@libdir@/build_info.txt
LOGS_DIR="@libdir@/build_logs"
LOG_FILE="$LOGS_DIR/server-build-dist.log"

cp_sys_lib()
{
    "$SOURCE_ROOT_PATH"/build_utils/copy_system_library -c "$COMPILER" "$@"
}

buildDistribution()
{
    echo "Creating directories"
    mkdir -p $BINSTAGE
    mkdir -p $LIBSTAGE
    mkdir -p $LIBPLUGINSTAGE
    mkdir -p $ETCSTAGE
    mkdir -p $SHARESTAGE
    mkdir -p $INITSTAGE
    mkdir -p $INITDSTAGE
    mkdir -p $SYSTEMDSTAGE

    echo "Copying build_info.txt"
    cp -r $BUILD_INFO_TXT $BINSTAGE/../

    if [ '@arch@' != 'arm' ]
    then
        echo "Copying dbsync 2.2"
        cp -r @packages.dir@/@rdep.target@/appserver-2.2.1/share/dbsync-2.2 $SHARESTAGE
        cp @libdir@/version.py $SHARESTAGE/dbsync-2.2/bin
    fi

    local LIB
    local LIB_BASENAME
    for LIB in "$SERVER_LIB_PATH"/*.so*
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

    # Copy mediaserver plugins.
    local PLUGIN_FILENAME
    local PLUGINS=( hikvision_metadata_plugin )
    if [ "$COMPANY_NAME" == "hanwha" ]
    then
        PLUGINS+=( hanwha_metadata_plugin )
    fi
    for PLUGIN in "${PLUGINS[@]}"
    do
        PLUGIN_FILENAME="lib$PLUGIN.so"
        echo "Copying (plugin) $PLUGIN_FILENAME"
        cp "$SERVER_LIB_PLUGIN_PATH/$PLUGIN_FILENAME" "$LIBPLUGINSTAGE/"
    done

    echo "Copying Festival VOX files"
    cp -r $SERVER_VOX_PATH $BINSTAGE

    cp_sys_lib libstdc++.so.6 "$LIBSTAGE"

    if [ '@arch@' != 'arm' ]
    then
        echo "Copying libicu"
        cp -P @qt.dir@/lib/libicu*.so* $LIBSTAGE
    fi

    QTLIBS="Core Gui Xml XmlPatterns Concurrent Network Sql WebSockets"
    for var in $QTLIBS
    do
        qtlib=libQt5$var.so
        echo "Copying (Qt) $qtlib"
        cp -P @qt.dir@/lib/$qtlib* $LIBSTAGE
    done

    # Strip and remove rpath
    if [[ ('@build.configuration@' == 'release' || '@CMAKE_BUILD_TYPE@' == 'Release') \
        && '@arch@' != 'arm' ]]
    then
        for f in `find $LIBPLUGINSTAGE -type f`
        do
            echo "Stripping $f"
            strip $f
        done

        for f in `find $LIBSTAGE -type f`
        do
            echo "Stripping $f"
            strip $f
        done
    fi

    echo "Setting permissions"
    find $PKGSTAGE -type d -print0 | xargs -0 chmod 755
    find $PKGSTAGE -type f -print0 | xargs -0 chmod 644
    chmod -R 755 $BINSTAGE
    if [ '@arch@' != 'arm' ]
    then
        chmod 755 $SHARESTAGE/dbsync-2.2/bin/{dbsync,certgen}
    fi

    echo "Copying mediaserver binaries and scripts"
    install -m 755 $SERVER_BIN_PATH/mediaserver $BINSTAGE/mediaserver-bin
    install -m 750 $SERVER_BIN_PATH/root_tool $BINSTAGE
    install -m 755 $SERVER_BIN_PATH/testcamera $BINSTAGE
    install -m 755 $SERVER_BIN_PATH/external.dat $BINSTAGE
    install -m 755 $SCRIPTS_PATH/config_helper.py $BINSTAGE
    install -m 755 $SCRIPTS_PATH/shell_utils.sh $BINSTAGE

    echo "Copying mediaserver startup script"
    install -m 755 bin/mediaserver $BINSTAGE

    echo "Copying upstart and sysv script"
    install -m 644 init/networkoptix-mediaserver.conf $INITSTAGE/$COMPANY_NAME-mediaserver.conf
    install -m 755 init.d/networkoptix-mediaserver $INITDSTAGE/$COMPANY_NAME-mediaserver
    install -m 644 systemd/networkoptix-mediaserver.service $SYSTEMDSTAGE/$COMPANY_NAME-mediaserver.service

    echo "Preparing DEBIAN dir"
    mkdir -p $STAGE/DEBIAN
    chmod 00775 $STAGE/DEBIAN

    INSTALLED_SIZE=`du -s $STAGE | awk '{print $1;}'`

    echo "Generating DEBIAN/control"
    cat debian/control.template \
        | sed "s/INSTALLED_SIZE/$INSTALLED_SIZE/g" \
        | sed "s/VERSION/$VERSION/g" \
        | sed "s/ARCHITECTURE/$ARCHITECTURE/g" \
        > $STAGE/DEBIAN/control

    echo "Copying DEBIAN dir files"
    install -m 755 debian/preinst $STAGE/DEBIAN
    install -m 755 debian/postinst $STAGE/DEBIAN
    install -m 755 debian/prerm $STAGE/DEBIAN
    install -m 644 debian/templates $STAGE/DEBIAN

    echo "Generating DEBIAN/md5sums"
    (cd $STAGE
        md5sum $(find * -type f | grep -v '^DEBIAN/') > DEBIAN/md5sums
        chmod 644 DEBIAN/md5sums
    )

    echo "Creating .deb $FINALNAME.deb"
    (cd $STAGEBASE
        fakeroot dpkg-deb -b $FINALNAME
    )

    local DEB
    for DEB in "$SERVER_DEB_PATH"/*
    do
        echo "Copying $DEB"
        cp -Rf "$DEB" "$STAGEBASE/"
    done

    echo "Generating finalname-server.properties"
    echo "server.finalName=$FINALNAME" >> finalname-server.properties

    # Copying filtered resources.
    local RESOURCE
    for RESOURCE in "deb"/*
    do
        echo "Copying filtered $(basename "$RESOURCE")"
        cp -r "$RESOURCE" "$STAGEBASE/"
    done

    echo "Creating .zip $UPDATE_NAME"
    (cd $STAGEBASE
        zip -r ./$UPDATE_NAME ./ -x ./$FINALNAME/**\* $FINALNAME/
    )

    if [ '@distribution_output_dir@' != '' ]
    then
        echo "Moving distribution .zip and .deb to @distribution_output_dir@"
        mv "$STAGEBASE/$UPDATE_NAME" "@distribution_output_dir@/"
        mv "$STAGEBASE/$FINALNAME.deb" "@distribution_output_dir@/"
    else
        echo "Moving distribution .zip to @project.build.directory@"
        mv "$STAGEBASE/$UPDATE_NAME" "@project.build.directory@/"
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
