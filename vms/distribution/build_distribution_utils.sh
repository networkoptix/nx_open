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

# Copy system libraries using copy_system_library.py.
#
# [in] SOURCE_DIR
# [in] COMPILER
# [in] CFLAGS
# [in] LDFLAGS
#
distrib_copySystemLibs() # dest_dir libs_to_copy...
{
    local -r DEST_DIR="$1" && shift

    "$SOURCE_DIR"/build_utils/linux/copy_system_library.py \
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
    local -r PLUGINS_DIR_NAME="$1" && shift
    local -r TARGET_DIR="$1" && shift

    mkdir -p "$TARGET_DIR/$PLUGINS_DIR_NAME"

    local PLUGIN
    local PLUGIN_FILENAME
    for PLUGIN in "$@"
    do
        PLUGIN_FILENAME="lib$PLUGIN.so"
        echo "  Copying $PLUGIN_FILENAME to $PLUGINS_DIR_NAME"
        cp "$BUILD_DIR/bin/$PLUGINS_DIR_NAME/$PLUGIN_FILENAME" "$TARGET_DIR/$PLUGINS_DIR_NAME/"
    done
}

distrib_createArchive() # archive dir command...
{
    local -r ARCHIVE="$1"; shift
    local -r DIR="$1"; shift

    rm -rf "$ARCHIVE" #< Avoid updating an existing archive.
    echo "  Creating $ARCHIVE"
    ( cd "$DIR" && "$@" "$ARCHIVE" * ) #< Subshell prevents "cd" from changing the current dir.
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
