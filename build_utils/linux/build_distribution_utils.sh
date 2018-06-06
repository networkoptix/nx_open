# Framework for creating .deb packages.
#
# All global symbols have prefix "distr_".

#--------------------------------------------------------------------------------------------------
# private

# [in] distr_EXTRA_HELP
#
distr_showHelp()
{
    echo "Options:"
    echo " -v, --verbose: Do not redirect output to a log file."
    echo " -k, --keep-work-dir: Do not delete work directory on success."
    if [[ ! -z $distr_EXTRA_HELP ]]
    then
        echo "$distr_EXTRA_HELP"
    fi
}

# [out] distr_KEEP_WORK_DIR
# [out] distr_VERBOSE
#
distr_parseArgs() # "$@"
{
    distr_KEEP_WORK_DIR=0
    distr_VERBOSE=0

    local -i ARG_N=0
    local ARG
    for ARG in "$@"
    do
        if [ "$ARG" = "-h" -o "$ARG" = "--help" ]
        then
            distr_showHelp
            exit 0
        elif [ "$ARG" = "-k" ] || [ "$ARG" = "--keep-work-dir" ]
        then
            distr_KEEP_WORK_DIR=1
        elif [ "$ARG" = "-v" ] || [ "$ARG" = "--verbose" ]
        then
            distr_VERBOSE=1
        elif [[ ! -z $distr_PARSE_EXTRA_ARGS_FUNC ]]
        then
            $distr_PARSE_EXTRA_ARGS_FUNC "$ARG" $ARG_N "$@"
        fi

        ((++ARG_N))
    done
}

# [out] distr_STDERR_FILE
#
distr_redirectOutputToLog() # log_file
{
    local -r LOG_FILE="$1" && shift

    echo "  See the log in $LOG_FILE"

    mkdir -p "$(dirname "$LOG_FILE")" #< In case log dir is not created yet.
    rm -rf "$LOG_FILE"

    # Redirect only stdout to the log file, leaving stderr as is, because there seems to be no
    # reliable way to duplicate error output to both log file and stderr, keeping the correct order
    # of regular and error lines in the log file.
    exec 1>"$LOG_FILE" #< Open log file for writing as stdout.

    distr_STDERR_FILE=$(mktemp)
    exec 2> >(tee "$distr_STDERR_FILE" >&2) #< Redirect stderr to a temporary file.
}

# Called by trap.
# [in] $?
# [in] distr_VERBOSE
# [in] distr_STDERR_FILE
# [in] distr_KEEP_WORK_DIR
# [in] distr_WORK_DIR
#
distr_onExit()
{
    local -r -i RESULT=$?

    if [ -s "$distr_STDERR_FILE" ] #< Whether the file exists and is not empty.
    then
        echo ""
        python -c 'print("-" * 99)' #< Print separator.
        echo "- stderr"
        echo ""
        cat "$distr_STDERR_FILE" #< Copy accumulated stderr to the log file.
    fi
    rm -rf "$distr_STDERR_FILE"

    echo "" #< Empty line to separate SUCCESS/FAILURE message from the above log.
    if [ $RESULT != 0 ]
    then #< Failure.
        local -r ERROR_MESSAGE="FAILURE (status $RESULT); see the error message(s) above."
        echo "$ERROR_MESSAGE" >&2 #< To stderr.
        if [ $distr_VERBOSE = 0 ] #< Whether stdout is redirected to the log file.
        then
            echo "$ERROR_MESSAGE" #< To log file.
        fi
    else
        echo "SUCCESS"

        if [ $distr_KEEP_WORK_DIR = 0 ]
        then
            rm -rf "$distr_WORK_DIR"
        fi
    fi

    return $RESULT
}

# Global variables - need to be accessible in onExit().
declare -g distr_WORK_DIR
declare -g -i distr_VERBOSE
declare -g -i distr_KEEP_WORK_DIR
declare -g distr_STDERR_FILE

#--------------------------------------------------------------------------------------------------
# public

# Description of additional command-line options to show with the help.
declare -g distr_EXTRA_HELP=""

# Callback which is called for each command-line argument:
#     parseExtraArgs arg_value arg_number "$@"
declare -g distr_PARSE_EXTRA_ARGS_FUNC=""

# [in,out] CONFIG If defined, overrides config_file, otherwise is set to config_file.
#
distr_loadConfig() # config_file
{
    : ${CONFIG="$1"} && shift #< CONFIG can be overridden externally.
    if [ ! -z "$CONFIG" ]
    then
        source "$CONFIG"
    fi
}

# Copy system library(ies) using copy_system_library.py.
#
# [in] SOURCE_DIR
# [in] COMPILER
# [in] CFLAGS
# [in] LDFLAGS
#
distr_cpSysLib() # dest_dir libs_to_copy...
{
    local -r DEST_DIR="$1" && shift

    "$SOURCE_DIR"/build_utils/linux/copy_system_library.py \
        --compiler="$COMPILER" \
        --flags="$CFLAGS" \
        --link-flags="$LDFLAGS" \
        --dest-dir="$DEST_DIR" \
        "$@"
}

# Prepare to build the distribution .deb package.
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
# [in] distr_EXTRA_HELP
#
distr_prepareToBuildDistribution() # work_dir log_file "$@"
{
    distr_WORK_DIR="$1" && shift
    local -r LOG_FILE="$1" && shift

    distr_parseArgs "$@"

    trap distr_onExit EXIT

    rm -rf "$distr_WORK_DIR"

    if [ distr_KEEP_WORK_DIR = 1 ]
    then
        local -r WORK_DIR_NOTE="(ATTENTION: will NOT be deleted)"
    else
        local -r WORK_DIR_NOTE="(will be deleted on success)"
    fi
    echo "Creating distribution in $distr_WORK_DIR $WORK_DIR_NOTE."

    distr_STDERR_FILE=""
    if [ $distr_VERBOSE = 0 ]
    then
        distr_redirectOutputToLog "$LOG_FILE"
    fi
}
