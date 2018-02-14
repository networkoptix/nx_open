#!/bin/bash

set -e

COMPILER=gcc

HELP_MESSAGE="Usage: $(basename $0) <library> <destination> [options]
Copy system library to a destination directory and create the nesessary symlinks.

Options:
  -c, --compiler    Compiler executable (default $COMPILER)
                    Used to determine system library location with <compiler> --print-file-name.
  -h, --help        Print this message"

function run_with_echo()
{
    echo "$@"
    "$@"
}

while [[ $# -gt 0 ]]
do
    case $1 in
        -c|--compiler)
        COMPILER="$2"
        shift
        shift
        ;;
        -h|--help)
        echo "$HELP_MESSAGE"
        exit
        ;;
        *)
        if [ -z $LIBRARY ]
        then
            LIBRARY="$1"
        elif [ -z $DESTINATION ]
        then
            DESTINATION="$1"
        else
            echo "Unknown argument $1"
            echo "$HELP_MESSAGE"
            exit 1
        fi
        shift
        ;;
    esac
done

if [ -z $LIBRARY ] || [ -z $DESTINATION ]
then
    echo "$HELP_MESSAGE"
    exit 1
fi

FILE=$("$COMPILER" --print-file-name "$LIBRARY")

if [ $FILE = $LIBRARY ]
then
    echo "$COMPILER failed to find $LIBRARY"
    exit 1
fi

while [ -L "$FILE" ]
do
    LINK="$(readlink $FILE)"
    if [[ "$LINK" = /* ]]
    then
        FILE="$LINK"
    else
        FILE="$(dirname $FILE)/$LINK"
    fi
done

if [ -L "$FILE" ]
then
    FILE_NAME=$(basename $(readlink "$FILE"))
elif [ -f "$FILE" ]
then
    FILE_NAME=$(basename "$FILE")
else
    echo "$FILE is not a file or symlink"
    exit 1
fi

run_with_echo cp -f "$FILE" "$DESTINATION/$FILE_NAME"

LINK_NAME="${FILE_NAME%.*}"
while [[ $LINK_NAME =~ \.[0-9]+ ]]
do
    run_with_echo ln -sf "$FILE_NAME" "$DESTINATION/$LINK_NAME"
    LINK_NAME="${LINK_NAME%.*}"
done
