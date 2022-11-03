#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Utilities helpful for creating distribution packages (.deb, .dmg or archives).
#
# All global symbols have prefix "distrib_".

#--------------------------------------------------------------------------------------------------
# private

# [in] distrib_EXTRA_HELP
#
distrib_showHelp()
{
    echo "Options:"
    echo " -v, --verbose: Do not redirect output to a log file."
    echo " -k, --keep-work-dir: Do not delete work directory on success."
    if [[ ! -z $distrib_EXTRA_HELP ]]
    then
        echo "$distrib_EXTRA_HELP"
    fi
}

# [out] distrib_KEEP_WORK_DIR
# [out] distrib_VERBOSE
#
distrib_parseArgs() # "$@"
{
    distrib_KEEP_WORK_DIR=0
    distrib_VERBOSE=0

    local -i ARG_N=0
    local ARG
    for ARG in "$@"
    do
        if [ "$ARG" = "-h" -o "$ARG" = "--help" ]
        then
            distrib_showHelp
            exit 0
        elif [ "$ARG" = "-k" ] || [ "$ARG" = "--keep-work-dir" ]
        then
            distrib_KEEP_WORK_DIR=1
        elif [ "$ARG" = "-v" ] || [ "$ARG" = "--verbose" ]
        then
            distrib_VERBOSE=1
        elif [[ ! -z $distrib_PARSE_EXTRA_ARGS_FUNC ]]
        then
            $distrib_PARSE_EXTRA_ARGS_FUNC "$ARG" $ARG_N "$@"
        fi

        ((++ARG_N))
    done
}

# [out] distrib_STDERR_FILE
#
distrib_redirectOutputToLog() # log_file
{
    local -r LOG_FILE="$1" && shift

    echo "  See the log in $LOG_FILE"

    mkdir -p "$(dirname "$LOG_FILE")" #< In case log dir is not created yet.
    rm -rf "$LOG_FILE"

    # Redirect only stdout to the log file, leaving stderr as is, because there seems to be no
    # reliable way to duplicate error output to both log file and stderr, keeping the correct order
    # of regular and error lines in the log file.
    exec 1>"$LOG_FILE" #< Open log file for writing as stdout.

    distrib_STDERR_FILE=$(mktemp)
    exec 2> >(tee "$distrib_STDERR_FILE" >&2) #< Redirect stderr to a temporary file.
}

# Called by trap.
# [in] $?
# [in] distrib_VERBOSE
# [in] distrib_STDERR_FILE
# [in] distrib_KEEP_WORK_DIR
# [in] distrib_WORK_DIR
#
distrib_onExit()
{
    local -r -i RESULT=$?

    set +x #< Disable logging each command (in case it was enabled previously with "set -x").

    if [ -s "$distrib_STDERR_FILE" ] #< Whether the file exists and is not empty.
    then
        echo ""
        python -c 'print("-" * 99)' #< Print separator.
        echo "- stderr"
        echo ""
        cat "$distrib_STDERR_FILE" #< Copy accumulated stderr to the log file.
    fi
    rm -rf "$distrib_STDERR_FILE"

    echo "" #< Empty line to separate SUCCESS/FAILURE message from the above log.
    if [ $RESULT != 0 ]
    then #< Failure.
        local -r ERROR_MESSAGE="FAILURE (status $RESULT); see the error message(s) above."
        echo "$ERROR_MESSAGE" >&2 #< To stderr.
        if [ $distrib_VERBOSE = 0 ] #< Whether stdout is redirected to the log file.
        then
            echo "$ERROR_MESSAGE" #< To log file.
        fi
    else
        echo "SUCCESS"

        if [ $distrib_KEEP_WORK_DIR = 0 ]
        then
            rm -rf "$distrib_WORK_DIR"
        fi
    fi

    return $RESULT
}

# Global variables - need to be accessible in onExit().
declare distrib_WORK_DIR
declare -i distrib_VERBOSE
declare -i distrib_KEEP_WORK_DIR
declare distrib_STDERR_FILE

#--------------------------------------------------------------------------------------------------
# public

# Description of additional command-line options to show with the help.
declare distrib_EXTRA_HELP=""

# Callback which is called for each command-line argument:
#     parseExtraArgs arg_value arg_number "$@"
declare distrib_PARSE_EXTRA_ARGS_FUNC=""

# [in,out] CONFIG If defined, overrides config_file, otherwise is set to config_file.
#
distrib_loadConfig() # config_file
{
    : ${CONFIG="$1"} && shift #< CONFIG can be overridden externally.
    if [ ! -z "$CONFIG" ]
    then
        source "$CONFIG"
    fi
}

# Read installed size from a DEB package.
#
# dpkg-deb will print the required info like "Installed-Size: 12345" where 12345 is approximate
# size in kylobytes.
#
# [in] DEB_FILE
#
distrib_getInstalledSizeFromDeb() # deb_file
{
    local -r DEB_FILE="$1" && shift

    local FREE_SPACE_REQUIRED=$(dpkg-deb -I "$DEB_FILE" |grep "Installed-Size:" |awk '{print $2}')
    FREE_SPACE_REQUIRED=$(($FREE_SPACE_REQUIRED * 1024))
    echo $FREE_SPACE_REQUIRED
}

# Copy system libraries using copy_system_library.py.
#
# [in] OPEN_SOURCE_DIR
# [in] COMPILER
# [in] CFLAGS
# [in] LDFLAGS
#
distrib_copySystemLibs() # dest_dir libs_to_copy...
{
    local -r DEST_DIR="$1" && shift

    "$PYTHON" "$OPEN_SOURCE_DIR"/build_utils/linux/copy_system_library.py \
        --compiler="$COMPILER" \
        --flags="$CFLAGS" \
        --link-flags="$LDFLAGS" \
        --dest-dir="$DEST_DIR" \
        "$@"
}

# Copy specified plugins from $BUILD_DIR/bin/plugins-dir-name to target-dir/plugins-dir-name>/.
# Create target-dir if needed.
#
# [in] BUILD_DIR
#
distrib_copyMediaserverPlugins() # plugins-folder-name target-dir plugin_lib_name...
{
    local -r PLUGINS_DIR_NAME="$1" && shift #< e.g.: plugins, plugins_optional
    local -r TARGET_DIR="$1" && shift

    mkdir -p "$TARGET_DIR/$PLUGINS_DIR_NAME"

    local PLUGIN
    local PLUGIN_FILENAME
    for PLUGIN in "$@"
    do
        PLUGIN_FILENAME="lib$PLUGIN.so"

        if [[ -f "$BUILD_DIR/bin/$PLUGINS_DIR_NAME/$PLUGIN_FILENAME" ]]
        then
            echo "  Copying $PLUGIN_FILENAME to $PLUGINS_DIR_NAME"
            cp "$BUILD_DIR/bin/$PLUGINS_DIR_NAME/$PLUGIN_FILENAME" "$TARGET_DIR/$PLUGINS_DIR_NAME/"
        elif [[ -d "$BUILD_DIR/bin/$PLUGINS_DIR_NAME/$PLUGIN" ]]
        then
            echo "  Copying $PLUGIN dedicated directory to $PLUGINS_DIR_NAME"
            cp -r "$BUILD_DIR/bin/$PLUGINS_DIR_NAME/$PLUGIN" "$TARGET_DIR/$PLUGINS_DIR_NAME/"
        else
            echo "ERROR: Cannot find plugin $PLUGIN" >&2
        fi
    done
}

# Copy the specified file to the proper location, creating the symlink if necessary: if
# ALT_LIB_INSTALL_PATH is unset, or the file is a symlink, just copy it to STAGE/LIB_INSTALL_PATH;
# otherwise, put it to STAGE/ALT_LIB_INSTALL_PATH, and create a symlink from
# STAGE/LIB_INSTALL_PATH to the file basename in /ALT_LIB_INSTALL_PATH. If an optional parameter
# library_subdirectory is passed to the function, it adds it as a suffix to both LIB_INSTALL_PATH
# and ALT_LIB_INSTALL_PATH.
#
# [in] STAGE
# [in] LIB_INSTALL_PATH
# [in] ALT_LIB_INSTALL_PATH Path to the alternative lib directory in the distribution creation
#      directory.
distrib_copyLib() # file [library_subdirectory]
{
    local -r file="$1"
    local -r lib_subdir="${2-}" #< The default value for lib_subdir is an empty string.

    # Create a temporary variable which value is the same as the value of the
    # ALT_LIB_INSTAL_PATH variable if it is set, or empty otherwise.
    local -r alt_lib_instal_path_local="${ALT_LIB_INSTALL_PATH-}"

    if [[ -z "${lib_subdir}" ]]; then
        local -r lib_dir="${LIB_INSTALL_PATH}"
        local -r alt_lib_dir="${alt_lib_instal_path_local}"
    else
        local -r lib_dir="${LIB_INSTALL_PATH}/${LIB_SUBDIR}"
        local -r alt_lib_dir="${alt_lib_instal_path_local}/${LIB_SUBDIR}"
    fi

    local -r stage_lib="${STAGE}/${lib_dir}"

    if [[ ! -z "${alt_lib_instal_path_local}" && ! -L "${FILE}" ]]; then
        # FILE is not a symlink - put to the alt location, and create a symlink to it.
        cp -r "${file}" "${STAGE}/${alt_lib_dir}/"
        local -r file_name=$(basename "${file}")
        ln -s "/${alt_lib_dir}/${file_name}" "${stage_lib}/${file_name}"
    else
        # FILE is a symlink, or the alt location is not defined - put to the regular location.
        cp -r "${file}" "${stage_lib}/"
    fi
}

# Copy Server libs to the distribution creation folder using distrib_copyLib() to ensure the
# correct directory structure (libraries and symlinks to them in different directories if
# necessary).
#
# ATTENTION: We explicitly add all the necessary libraries to the list, so this function is
# version-specific and must be reviewed every time when a new library is added to the Server
# distribution.
#
# [in] BUILD_DIR
# [in] STAGE
# [in] LIB_INSTALL_PATH
# [in] ALT_LIB_INSTALL_PATH
distrib_copyServerLibs() # additional_libs_to_copy...
{
    local -a additional_server_libs=("$@")

    echo ""
    echo "Copying libs"

    local -a libs_to_copy=(
        # vms
        libappserver2
        libcloud_db_client
        libnx_vms_server
        libnx_vms_server_db
        libnx_speech_synthesizer
        libnx_email
        libnx_fusion
        libnx_reflect
        libnx_kit
        libnx_branding
        libnx_build_info
        libnx_monitoring
        libnx_network
        libnx_utils
        libnx_vms_utils
        libnx_zip
        libnx_sql
        libnx_vms_api
        libnx_analytics_db
        libnx_vms_common
        libnx_vms_rules
        libnx_vms_update
        libonvif
        libnx_onvif
        libnx_ldap
        libnx_vms_json_rpc
        libsrtp2

        # third-party
        libquazip
        libudt

        # additional libraries
        "${additional_server_libs[@]}"
    )

    local -r optional_libs_to_copy=(
        libvpx
    )

    # OpenSSL.
    libs_to_copy+=(
        libssl
        libcrypto
    )

    mkdir -p "${STAGE}/${LIB_INSTALL_PATH}"
    # if ALT_LIB_INSTALL_PATH is not set, consider it to be an empty string.
    if [[ ! -z "${ALT_LIB_INSTALL_PATH-}" ]]; then
        mkdir -p "${STAGE}/${ALT_LIB_INSTALL_PATH}"
    fi

    local lib
    for lib in "${libs_to_copy[@]}"; do
        local file
        for file in "${BUILD_DIR}/lib/${lib}".so*; do
            if [[ "${file}" != *.debug ]]; then
                echo "    Copying $(basename "${file}")"
                distrib_copyLib "${file}"
            fi
        done
    done
    for lib in "${optional_libs_to_copy[@]}"; do
        local file
        for file in "${BUILD_DIR}/lib/${lib}".so*; do
            if [[ -f "${file}" ]]; then
                echo "    Copying (optional) $(basename "${file}")"
                distrib_copyLib "${file}"
            fi
        done
    done
}

# Copy ffmpeg libs to the distribution creation directory.
#
# [in] STAGE
# [in] LIB_INSTALL_PATH
# [in] ALT_LIB_INSTALL_PATH
distrib_copyFfmpegLibs() # source_directory [distribution_libs_ffmpeg_subdirectory]
{
    local -r source_directory="$1"
    local -r ffmpeg_subdirectory="${2-}" #< The default value for this argument is an empty string.

    local -r ffmpeg_libs=(
        libavcodec.so.58
        libavfilter.so.7
        libavformat.so.58
        libavutil.so.56
        libswscale.so.5
        libswresample.so.3
        libavdevice.so.58
    )

    if [[ ! -z "${ffmpeg_subdirectory}" ]]; then
        mkdir -p "${STAGE}/${LIB_INSTALL_PATH}/${ffmpeg_subdirectory}"
        # if ALT_LIB_INSTALL_PATH is not set, consider it to be an empty string.
        if [[ ! -z "${ALT_LIB_INSTALL_PATH-}" ]]; then
            mkdir -p "${STAGE}/${ALT_LIB_INSTALL_PATH}/${ffmpeg_subdirectory}"
        fi
    fi

    for lib in "${ffmpeg_libs[@]}"; do
        local file
        for file in "${source_directory}/${lib}"*; do
            if [ -f "${file}" ]; then
                echo "Copying $(basename "${file}")"
                distrib_copyLib "${file}" "${ffmpeg_subdirectory}"
            fi
        done
    done
}

# Copy Qt libs to the distribution creation directory.
#
# [in] QT_DIR
distrib_copyQtLibs() # additional_libs_to_copy...
{
    local -a additional_qt_libs=("$@")

    echo ""
    echo "Copying Qt libs"

    local qt_libs_to_copy=(
        Concurrent
        Core
        Gui
        Xml
        Network
        Sql
        WebSockets
        Qml

        "${additional_qt_libs[@]}"
    )

    local qt_lib
    for qt_lib in "${qt_libs_to_copy[@]}"; do
        local lib_filename="libQt5${qt_lib}.so"
        echo "  Copying (Qt) ${lib_filename}"
        local file
        for file in "${QT_DIR}/lib/${lib_filename}"*; do
            distrib_copyLib "${file}"
        done
    done
}

# Copy Qt plugins to the distribution creation directory.
#
# [in] STAGE
# [in] MODULE_INSTALL_PATH
# [in] QT_DIR
distrib_copyQtPlugins() # additional_plugins_to_copy...
{
    local -a plugins=("$@")

    echo ""
    echo "Copying Qt plugins for mediaserver"

    local -r qt_plugins_install_dir="${STAGE}/${MODULE_INSTALL_PATH}/plugins"
    mkdir -p "${qt_plugins_install_dir}"

    local plugin
    for plugin in "${plugins[@]}"; do
        echo "  Copying (Qt plugin) ${plugin}"

        mkdir -p "${qt_plugins_install_dir}/$(dirname "${plugin}")"
        cp -r "${QT_DIR}/plugins/${plugin}" "${qt_plugins_install_dir}/${plugin}"
    done
}

# Copy server binaries and other files that must reside next to them to the distribution creation
# directory.
#
# [in] STAGE
# [in] BIN_INSTALL_PATH
# [in] BUILD_DIR
# [in] CURRENT_BUILD_DIR
# [in] ENABLE_ROOT_TOOL
# [in] OPEN_SOURCE_DIR
distrib_copyServerBins()
{
    echo ""
    echo "Copying mediaserver binaries"

    local -r stage_bin="${STAGE}/${BIN_INSTALL_PATH}"
    mkdir -p "${stage_bin}"

    install -m 755 "${BUILD_DIR}/bin/mediaserver" "${stage_bin}/"
    install -m 755 "${BUILD_DIR}/bin/external.dat" "${stage_bin}/" #< TODO: Why "+x" is needed?

    echo "Copying translations"
    install -m 755 -d "${stage_bin}/translations"
    install -m 644 "${BUILD_DIR}/bin/translations/nx_vms_common.dat" "${stage_bin}/translations/"
    install -m 644 "$BUILD_DIR/bin/translations/nx_vms_rules.dat" "$STAGE_BIN/translations/"
    install -m 644 "$BUILD_DIR/bin/translations/nx_vms_server.dat" "$STAGE_BIN/translations/"
    install -m 644 "$BUILD_DIR/bin/translations/nx_vms_server_db.dat" "$STAGE_BIN/translations/"

    # if ENABLE_ROOT_TOOL is not set, consider it to be "false".
    if [[ "${ENABLE_ROOT_TOOL-"false"}" == "true" ]]; then
        echo "Copying root-tool"
        install -m 750 "${BUILD_DIR}/bin/root_tool" "${stage_bin}/"
        install -m 640 "${CURRENT_BUILD_DIR}/system_commands.conf" "${stage_bin}/"
    fi

    echo "Copying nx_log_viewer.html"
    install -m 755 "${OPEN_SOURCE_DIR}/nx_log_viewer.html" "${stage_bin}/"

    echo "Copying qt.conf"
    install -m 644 "${CURRENT_BUILD_DIR}/qt.conf" "${stage_bin}/"


    echo "Copying configuration scripts"
    install -m 755 "$SCRIPTS_DIR/config_helper.sh" "$STAGE_BIN/"
    install -m 755 "$SCRIPTS_DIR/upgrade_config.sh" "$STAGE_BIN/"
}

distrib_createArchive() # archive dir command...
{
    local -r archive="$1"; shift
    local -r dir="$1"; shift

    rm -rf "${archive}" #< Avoid updating an existing archive.
    echo "  Creating ${archive}"
    ( cd "${dir}" && "$@" "${archive}" * ) #< Subshell prevents "cd" from changing the current dir.
    echo "  Done"
}

# Prepare to build the distribution archives/packages.
#
# The specified work_dir will be deleted on exit (if not prohibited by the command-line args), but
# should be created after calling this function.
#
# Redirect the output to log_file, if the verbose mode is not requested. Stderr is duplicated
# to the actual stderr, and also accumulated in a file and finally appended to the log file.
#
# Parse common command-line args, in particular:
# - Provide help if needed.
# - Suppress output redirection (verbose mode).
# - Do not delete work_dir on exit (useful for debugging).
#
# [in] distrib_EXTRA_HELP
#
distrib_prepareToBuildDistribution() # work_dir log_file "$@"
{
    distrib_WORK_DIR="$1" && shift
    local -r LOG_FILE="$1" && shift

    distrib_parseArgs "$@"

    trap distrib_onExit EXIT

    rm -rf "$distrib_WORK_DIR"

    if [ distrib_KEEP_WORK_DIR = 1 ]
    then
        local -r WORK_DIR_NOTE="(ATTENTION: will NOT be deleted)"
    else
        local -r WORK_DIR_NOTE="(will be deleted on success)"
    fi
    echo "Creating distribution in $distrib_WORK_DIR $WORK_DIR_NOTE."

    distrib_STDERR_FILE=""
    if [ $distrib_VERBOSE = 0 ]
    then
        distrib_redirectOutputToLog "$LOG_FILE"
    fi
}
