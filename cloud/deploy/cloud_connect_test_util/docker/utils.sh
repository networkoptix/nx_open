#!/bin/bash

# Utilities to be used in various Nx bash scripts. Contains only nx...() functions and NX_... vars.

# TODO: Rename functions using camelStyle.

#--------------------------------------------------------------------------------------------------
# Low-level utils.

# Part of help text describing common options handled by nx_run().
declare -g -r NX_HELP_TEXT_OPTIONS="$(cat \
<<EOF
Here <options> is a combination of the following options, in the listed order:
 -h, --help # Show this help. Also shown when called without arguments.
 -v, --verbose # Log execution of this script via "set -x".
 -gov, --go-verbose # Log commands to be executed remotely via nx_go() (ssh or telnet).
 --mock-rsync # Do not call real rsync, just log its calling commands.
EOF
)"

# Call help_callback() and exit the script if none or typical help command-line arguments are
# specified.
nx_handle_help() # "$@"
{
    if [ $# = 0 -o "$1" = "-h" -o "$1" = "--help" ]; then
        help_callback
        exit 0
    fi
}

# Set the verbose mode if required by $1; return whether $1 is consumed: define and set global var
# NX_VERBOSE to either 0 or 1.
nx_handle_verbose() # "$@" && shift
{
    if [ "$1" = "--verbose" -o "$1" = "-v" ]; then
        declare -g -r -i NX_VERBOSE=1
        set -x
        return 0
    else
        declare -g -r -i NX_VERBOSE=0
        return 1
    fi
}

# Set the verbose mode for nx_go() if required by $1; return whether $1 is consumed: define and set
# global var NX_GO_VERBOSE to either 0 or 1.
nx_handle_go_verbose() # "$@" && shift
{
    if [ "$1" = "-gov" -o "$1" = "--go-verbose" ]; then
        declare -g -r -i NX_GO_VERBOSE=1
        return 0
    else
        declare -g -r -i NX_GO_VERBOSE=0
        return 1
    fi
}

# Set the mode to simulate rsync calls and return whether $1 is consumed.
nx_handle_mock_rsync() # "$@" && shift
{
    if [ "$1" == "--mock-rsync" ]; then
        rsync() #< Define the function which overrides rsync executable name.
        {
            nx_echo
            nx_echo "MOCKED:"
            nx_echo "rsync $*"
            nx_echo
            # Check that all local files exist.
            local FILE
            local -i I=0
            for FILE in "$@"; do
                 # Check all args not starting with "-" except the last, which is the remote path.
                let I='I+1'
                if [[ $I < $# && $FILE != \-* ]]; then
                    if [ ! -r "$FILE" ]; then
                        nx_fail "Mocked rsync: Cannot access local file: $FILE"
                    fi
                fi
            done
        }
        return 0
    else
        return 1
    fi
}

nx_check_args() # N "$@"
{
    if (( $# < 1 ))
    then
        nx_fail "INTERNAL ERROR: Not enough args for ${FUNCNAME[0]}() called from ${FUNCNAME[1]}."
    fi

    local -r -i N="$1"
    shift

    if (( $# < $N ))
    then
        nx_fail "INTERNAL ERROR: Not enough args for ${FUNCNAME[1]}."
    fi
}

# Copy the file(s) recursively, showing a progress.
nx_rsync() # rsync_args...
{
    rsync -rlpDh --progress "$@"
}

# Log the args if in verbose mode, otherwise, do nothing.
nx_log() # ...
{
    # Allow "set -x" to echo the args. If not called under "set -x", do nothing, but suppress
    # unneeded logging (bash does not allow to define an empty function, and if ":" is used, it
    # will be logged by "set -x".)
    {
        set +x;
        [ $NX_VERBOSE = 1 ] && set -x
    } 2>/dev/null
}

# Log var name and value if in verbose mode, otherwise, do nothing.
nx_log_var() # VAR_NAME
{
    # Allow "set -x" to echo the args. If not called under "set -x", do nothing, but suppress
    # unneeded logging (bash does not allow to define an empty function, and if ":" is used, it
    # will be logged by "set -x".)
    {
        set +x;

        if [ $NX_VERBOSE = 1 ]
        then
            local -r VAR_NAME="$1"
            eval local -r VAR_VALUE="\$$VAR_NAME"
            echo "####### $VAR_NAME: [$VAR_VALUE]"

            set -x
        fi
    } 2>/dev/null
}

# Log array var name and values if in verbose mode, otherwise, do nothing.
nx_log_array() # ARRAY_NAME
{
    # Allow "set -x" to echo the args. If not called under "set -x", do nothing, but suppress
    # unneeded logging (bash does not allow to define an empty function, and if ":" is used, it
    # will be logged by "set -x".)
    {
        set +x;

        if [ $NX_VERBOSE = 1 ]
        then
            local -r ARRAY_NAME="$1"
            eval local -r -a ARRAY_VALUE=("\${$ARRAY_NAME[@]}")
            echo "####### $ARRAY_NAME: array[${#ARRAY_VALUE[@]}]:"
            echo "####### {"
            local ITEM_VALUE
            for ITEM_VALUE in "${ARRAY_VALUE[@]}"
            do
                echo "#######     [$ITEM_VALUE]"
            done
            echo "####### }"

            set -x
        fi
    } 2>/dev/null
}

# Log map (dictionary) var name and key-values if in verbose mode, otherwise, do nothing.
nx_log_map() # ARRAY_NAME
{
    # Allow "set -x" to echo the args. If not called under "set -x", do nothing, but suppress
    # unneeded logging (bash does not allow to define an empty function, and if ":" is used, it
    # will be logged by "set -x".)
    {
        set +x;

        if [ $NX_VERBOSE = 1 ]
        then
            local -r MAP_NAME="$1"
            # Copy a map with name $MAP_NAME to the local map MAP_VALUE.
            local -r COMMAND=$( \
                typeset -A -p $MAP_NAME `# Print "declare -A $MAP_NAME=([key]="value"...)" #` \
                    |sed "s/$MAP_NAME=/MAP_VALUE=/" \
                    |sed 's/declare /local /' \
                    |sed "s/'//g" `# Workaround for a bash bug which adds apostrophes. #` \
            )
            eval "$COMMAND"
            echo "####### $MAP_NAME: map[${#MAP_VALUE[@]}]:"
            echo "####### {"
            local KEY
            for KEY in "${!MAP_VALUE[@]}"
            do
                echo "#######     [$KEY] -> [${MAP_VALUE["$KEY"]}]"
            done
            echo "####### }"

            set -x
        fi
    } 2>/dev/null
}

# Log the contents of the file in verbose mode via "sudo cat", otherwise, do nothing.
nx_log_file_contents() # filename
{
    # Args already echoed if called under "set -x", thus, do nothing but suppress unneeded logging.
    {
        set +x;
        local FILE="$1"
        if [ $NX_VERBOSE = 1 ]; then
            echo "<<EOF"
            sudo cat "$FILE"
            echo "EOF"
            set -x
        fi
    } 2>/dev/null
}

# Execute the command specified in the args, logging the call with "set -x", unless "set -x" mode
# is already on - in this case, the call of this function with all its args is already logged.
nx_verbose() # "$@"
{
    {
        if [ $NX_VERBOSE = 0 ]; then
            set -x
        else
            set +x
        fi
    } 2>/dev/null

    "$@"

    {
        local RESULT=$?
        if [ $NX_VERBOSE = 0 ]; then
            set +x
        else
            set -x
        fi
        return $RESULT
    } 2>/dev/null
}

nx_echo_value() # VAR_NAME
{
    local -r VAR_NAME="$1"
    eval local -r VAR_VALUE="\$$VAR_NAME"
    echo "####### $VAR_NAME: [$VAR_VALUE]"
}

# Echo the args replacing full home path with '~', but do nothing in verbose mode because the args
# are already printed by "set -x".
nx_echo() # ...
{
    { set +x; } 2>/dev/null
    if [ $NX_VERBOSE = 0 ]; then
        echo "$@" |sed "s#$HOME/#~/#g"
    else
        set -x
    fi
}

# Echo the message and additional lines (if any) to stderr and exit the whole script with error.
nx_fail()
{
    nx_echo "ERROR: $1" >&2
    shift

    for LINE in "$@"; do
        nx_echo "    $LINE" >&2
    done
    exit 1
}

# Set the specified variable to the terminal background color (hex), or empty if not supported.
nx_get_background() # RRGGBB_VAR
{
    local RRGGBB_VAR="$1"

    eval "$RRGGBB_VAR="

    # To get terminal background, one should type "\033]11;?\033\\", and the terminal echoes:
    # "\033]11;rgb:RrRr/GgGg/BbBb\033\\", where Rr is Red hex, Gg is Green hex, and Bb is Blue hex.
    # If the terminal does not support it, it will not echo anything in reply.
    exec </dev/tty # Redirect chars generated by the terminal to stdin.
    local OLD_STTY_SETTINGS=$(stty -g)
    stty -echo #< Ask the terminal not to echo the background color request.
    echo -en "\033]11;?\033\\" #< Feed the terminal with the background color request.
    if read -r -d '\' -t 0.05 COLOR; then #< Read the generated chars (if any, use timeout 50 ms).
        eval "$RRGGBB_VAR=\${COLOR:11:2}\${COLOR:16:2}\${COLOR:21:2}"
    fi
    stty "$OLD_STTY_SETTINGS"
}

# Set the terminal background color (hex).
nx_set_background() # RRGGBB
{
    local RRGGBB="$1"

    [ ! -z "$RRGGBB" ] && echo -en "\\e]11;#${RRGGBB}\\a"
}

# Produce colored output: nx_echo "$(nx_red)Red text$(nx_nocolor)"
nx_nocolor()  { echo -en "\033[0m"; }
nx_black()    { echo -en "\033[0;30m"; }
nx_dred()     { echo -en "\033[0;31m"; }
nx_dgreen()   { echo -en "\033[0;32m"; }
nx_dyellow()  { echo -en "\033[0;33m"; }
nx_dblue()    { echo -en "\033[0;34m"; }
nx_dmagenta() { echo -en "\033[0;35m"; }
nx_dcyan()    { echo -en "\033[0;36m"; }
nx_lgray()    { echo -en "\033[0;37m"; }
nx_dgray()    { echo -en "\033[1;30m"; }
nx_lred()     { echo -en "\033[1;31m"; }
nx_lgreen()   { echo -en "\033[1;32m"; }
nx_lyellow()  { echo -en "\033[1;33m"; }
nx_lblue()    { echo -en "\033[1;34m"; }
nx_lmagenta() { echo -en "\033[1;35m"; }
nx_lcyan()    { echo -en "\033[1;36m"; }
nx_white()    { echo -en "\033[1;37m"; }

# Set the terminal window title.
nx_set_title()
{
    local TITLE="$*"

    echo -en "\033]0;${TITLE}\007"
}

# Save the current terminal title on the stack.
nx_push_title()
{
    echo -en "\033[22;0t"
}

# Restore the terminal title from the stack.
nx_pop_title()
{
    echo -en "\033[23;0t"
}

# Save the current cursor position.
# ATTENTION: Scrolling does not adjust the saved position, thus, restoring will not be possible.
nx_save_cursor_pos()
{
    echo -en "\033[s"
}

# Sequence to echo to the terminal to restore saved cursor position.
declare -r NX_RESTORE_CURSOR_POS="\033[u"

# Restore saved cursor position.
nx_restore_cursor_pos()
{
    echo -en "$NX_RESTORE_CURSOR_POS"
}

nx_absolute_path() # path
{
    readlink -f "$1"
}

# Change directory verbously, but only if the current dir is not the desired one.
nx_cd() # dir
{
    nx_check_args 1 "$@"
    local -r DIR="$1"
    if [ "$(nx_absolute_path "$(pwd)")" != "$(nx_absolute_path "$DIR")" ]
    then
        nx_verbose cd "$DIR"
    fi
}

nx_pushd() # "$@"
{
    # Do not print pushed dir name.
    # On failure, the error message is printed by pushd.
    pushd "$@" >/dev/null || nx_fail
}

nx_popd()
{
    # Do not print popped dir name.
    popd >/dev/null || nx_fail
}

# Concatenate and escape the args except "*" and args in "[]".
nx_concat_ARGS() # "$@"
{
    ARGS=""
    if [ ! -z "$*" ]; then
        local ARG
        for ARG in "$@"; do
            case "$ARG" in
                "["*"]") # Anything in square brackets.
                    ARGS+="${ARG:1:-1} " #< Trim surrounding braces.
                    ;;
                *)
                    local ARG_ESCAPED
                    printf -v ARG_ESCAPED "%q " "$ARG" #< Perform the escaping.
                    ARGS+="${ARG_ESCAPED//\\\*/*}" #< Append, unescaping all "*".
                    ;;
            esac
        done
        ARGS="${ARGS%?}" #< Trim the last space introduced by printf.
    fi
}

nx_is_cygwin()
{
    case "$(uname -s)" in
        CYGWIN*|MINGW*) return 0;;
        *) return 1;;
    esac
}

# If cygwin, convert cygwin path to windows path for external tools; otherwise, return path as is.
nx_path() # "$@"
{
    if nx_is_cygwin; then
        cygpath -w "$@"
    else
        echo "$@"
    fi
}

# If cygwin, convert windows or unix path to unix path; otherwise, return path as is.
nx_unix_path() # "$@"
{
    if nx_is_cygwin; then
        cygpath -u "$@"
    else
        echo "$@"
    fi
}

#--------------------------------------------------------------------------------------------------
# High-level utils, can use low-level utils.

# Call in case of invalid arguments. Prints an appropriate help message.
nx_fail_on_invalid_arguments()
{
    nx_fail "Invalid arguments. Run with -h for help."
}

nx_telnet() # user password host port terminal_title background_rrggbb [command [args...]]
{
    local USER="$1"; shift
    local PASSWORD="$1"; shift
    local HOST="$1"; shift
    local PORT="$1"; shift
    local TERMINAL_TITLE="$1"; shift
    local BACKGROUND_RRGGBB="$1"; shift
    local ARGS
    nx_concat_ARGS "$@"

    local OLD_BACKGROUND
    nx_get_background OLD_BACKGROUND
    nx_set_background "$BACKGROUND_RRGGBB"
    nx_push_title
    nx_set_title "$TERMINAL_TITLE"

    local -r PROMPT="#"
    local -r RESULT_FILE="$(mktemp)"

    expect -c "$(cat <<EOF
        log_user 0
        set timeout -1
        spawn telnet "$BOX_HOST" "$BOX_PORT"
        expect "login:" { send "$USER\\r" }
        expect "Password:" { send "$PASSWORD\\r" }
        if {"$ARGS" != ""} {
            expect "$PROMPT"
            log_user 1
            send "$ARGS\\r"
            expect "$PROMPT"
            set FD [open "$RESULT_FILE" w]
            log_user 0
            send "echo \$?\\r" ;#< Echo command exit status.
            expect -re {(\\d+)} { puts \$FD "\$expect_out(1,string)" } ;#< Print status to file.
            close \$FD
            puts "\\r"
        } else {
            expect "$PROMPT"
            send "alias l='ls -l'\\r"
            interact
        }
EOF
    )"

    local -r RESULT=$(cat "$RESULT_FILE")
    rm "$RESULT_FILE"

    nx_pop_title
    nx_set_background "$OLD_BACKGROUND"
    return $RESULT
}

# Execute a command at the box or log in to the box, via calling go_callback().
nx_go() # "$@"
{
    if [ $NX_GO_VERBOSE = 1 ]; then
        nx_go_verbose "$@"
    else
        go_callback "$@"
    fi
}

# Same as nx_go(), but logging the command to be called regardless of NX_GO_VERBOSE value.
nx_go_verbose() # "$@"
{
    local ARGS
    nx_concat_ARGS "$@"
    echo "+go $ARGS"

    go_callback "$@"

    # Alternative implementation involving "set -x" at the box.
    #go \
    #    "[ set -x; nxLogOffKeepingStatus(){ local -r R=\$?; set +x; return \$R; }; ]" \
    #    "$@" \
    #    "[ ; { nxLogOffKeepingStatus; } 2>/dev/null ]"
}

# Execute a command via ssh, or log in via ssh.
#
# Ssh reparses the concatenated args string at the remote host, thus, this function performs the
# escaping of the args, except for the "*" chars (to enable globs) and args in square brackets (to
# enable e.g. combining commands via "[&&]" or redirecting with "[>]").
nx_ssh() # user password host port terminal_title background_rrggbb [command [args...]]
{
    local USER="$1"; shift
    local PASSWORD="$1"; shift
    local HOST="$1"; shift
    local PORT="$1"; shift
    local TERMINAL_TITLE="$1"; shift
    local BACKGROUND_RRGGBB="$1"; shift
    local ARGS
    nx_concat_ARGS "$@"

    local OLD_BACKGROUND
    nx_get_background OLD_BACKGROUND
    nx_set_background "$BACKGROUND_RRGGBB"
    nx_push_title
    nx_set_title "$TERMINAL_TITLE"

    sshpass -p "$PASSWORD" ssh -q -p "$PORT" -t "$USER@$HOST" \
        `# Do not use known_hosts #` -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no \
        ${ARGS:+"$ARGS"} `#< Omit arg if empty #`
    local RESULT=$?

    nx_pop_title
    nx_set_background "$OLD_BACKGROUND"
    return $RESULT
}

nx_sshfs() # user password host port host_path mnt_point
{
    local USER="$1"; shift
    local PASSWORD="$1"; shift
    local HOST="$1"; shift
    local PORT="$1"; shift
    local HOST_PATH="$1"; shift
    local MNT_POINT="$1"; shift

    echo "$BOX_PASSWORD" |sshfs -p "$PORT" "$USER@$HOST":"$HOST_PATH" "$MNT_POINT" \
        -o nonempty,password_stdin \
        -o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no `# Do not use known_hosts`
}

nx_get_SELF_IP() # subnet-regex
{
    local -r SUBNET="$1"

    local AWK
    local TOOL
    case "$(uname -s)" in
        CYGWIN*) TOOL="ipconfig"; AWK='/  IPv4 Address/{print $14}';;
        *) TOOL="ifconfig"; AWK='/inet addr/{print substr($2,6)}';;
    esac

    SELF_IP=$($TOOL |awk "$AWK" |grep "$SUBNET")
    # Using the first line only. and cut possible trailing '\r' on Windows.
    SELF_IP=${SELF_IP%%$'\n'*}
    SELF_IP=${SELF_IP%%$'\r'*}

    if ! [[ $SELF_IP =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        nx_fail "Self IP address \"$SELF_IP\" does not match subnet regex \"$SUBNET\"."
    fi
}

# Return in the specified variable the array of files found by 'find' command.
nx_find_files() # FILES_ARRAY_VAR find_args...
{
    local FILES_ARRAY_VAR="$1"; shift

    local FILES_ARRAY=()
    while IFS="" read -r -d $'\0'; do
        FILES_ARRAY+=("$REPLY")
    done < <(find "$@" -print0)
    eval "$FILES_ARRAY_VAR"'=("${FILES_ARRAY[@]}")'
}

# Search for the file inside the given dir via 'find'. If more than one file is found, fail with
# the message including the file list.
nx_find_file() # FILE_VAR file_description_for_error_message dir find_args...
{
    local FILE_VAR="$1"; shift
    local FILE_DESCRIPTION="$1"; shift
    local FILE_LOCATION="$1"; shift

    local FILES=()
    nx_find_files FILES "$FILE_LOCATION" "$@"

    # Make sure "find" returned exactly one file.
    if [ ${#FILES[*]} = 0 ]; then
        nx_fail "Unable to find $FILE_DESCRIPTION in $FILE_LOCATION"
    fi
    if [ ${#FILES[*]} -gt 1 ]; then
        nx_fail "Found ${#FILES[*]} files for $FILE_DESCRIPTION matching [$@] in $FILE_LOCATION:" \
            "${FILES[@]}"
    fi

    eval "$FILE_VAR=\${FILES[0]}"
}

# If the specified variable is not set, scan from the current dir upwards up to but not including
# the specified parent dir, and return its child dir in the variable.
# @param error_message_if_not_found If specified, on failure a fatal error is produced, otherwise,
#     return 1 and set DIR_VAR to the current dir.
nx_find_parent_dir() # DIR_VAR parent/dir [error_message_if_not_found]
{
    local DIR_VAR="$1"
    local PARENT_DIR="$2"
    local ERROR_MESSAGE="$3"

    local DIR=$(eval "echo \$$DIR_VAR")

    if [ "$DIR" != "" ]; then
        return 0
    fi

    DIR=$(pwd)
    while [ "$(basename "$(dirname "$DIR")")" != "$PARENT_DIR" -a "$DIR" != "/" ]; do
        DIR=$(dirname "$DIR")
    done

    local RESULT=0
    if [ "$DIR" = "/" ]; then
        if [ -z "$ERROR_MESSAGE" ]; then
            RESULT=1
            DIR=$(pwd)
        else
            nx_fail "$ERROR_MESSAGE"
        fi
    fi

    eval "$DIR_VAR=\$DIR"
    return $RESULT
}

# Check that the specified file exists. Needed to support globs in the filename.
nx_glob_exists()
{
    [ -e "$1" ]
}

# Execute "sudo dd" showing a progress, return the status of "dd".
nx_sudo_dd() # dd_args...
{
    # Redirect to a subshell to enable capturing the pid of the "sudo" process.
    # Print only lines containing "copied", suppress '\n' (to avoid scrolling).
    # Spaces are added to overwrite the remnants of a previous text.
    # "-W interactive" runs awk without input buffering for certain versions of awk; suppressing
    # awk's stderr is used to avoid its warning in case it does not support this option.
    sudo dd "$@" 2> >(awk -W interactive \
        "\$0 ~ /copied/ {printf \"${NX_RESTORE_CURSOR_POS}%s                 \", \$0}" \
        2>/dev/null) &
    local SUDO_PID=$!

    # On ^C, kill "dd" which is the child of "sudo".
    trap "trap - SIGINT; sudo pkill -9 -P $SUDO_PID" SIGINT

    nx_save_cursor_pos
    while sudo kill -0 $SUDO_PID; do #< Checking that "sudo dd" is still running.
        # Ask "dd" to print the progress; break if "dd" is finished.
        sudo kill -USR1 $SUDO_PID || break

        sleep 1
    done

    # Avoid "%" appearing in the console.
    echo

    wait $SUDO_PID #< Get the Status Code of finished "dd".
}

# Source the specified file (typically with settings), return whether it exists.
nx_load_config() # "${CONFIG='.<tool-name>rc'}"
{
    local FILE="$1"

    local PATH="$HOME/$FILE"
    if [ -f "$PATH" ]; then
        source "$PATH"
    else
        return 1
    fi
}

# Called by trap.
nx_detail_on_exit()
{
    local RESULT=$?
    if [ $RESULT != 0 ]; then
        nx_echo "The script FAILED (status $RESULT)." >&2
    fi
    return $RESULT
}

# Call after sourcing this script.
nx_run()
{
    trap nx_detail_on_exit EXIT

    nx_handle_verbose "$@" && shift
    nx_handle_go_verbose "$@" && shift
    nx_handle_help "$@"
    nx_handle_mock_rsync "$@" && shift

    main "$@"
}

#--------------------------------------------------------------------------------------------------
# Conditional functions

if nx_is_cygwin
then
    sudo() #< Command "sudo" is missing on cygwin.
    {
        "$@"
    }
fi
