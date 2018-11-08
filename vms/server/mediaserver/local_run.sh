#!/bin/bash
set -x

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

export LD_LIBRARY_PATH=$DIR/opt/networkoptix/mediaserver/lib
export VMS_PLUGIN_DIR=$DIR/opt/networkoptix/mediaserver/lib/plugins

BINARY="$DIR/opt/networkoptix/mediaserver/bin/mediaserver-bin -e"
LOG="$DIR/valgrind.out.%p"

if [ $M ]; then
  VALGRIND="valgrind --tool=massif"
elif [ $D ]; then
  VALGRIND="valgrind --tool=exp-dhat --show-top-n=10000 --sort-by=max-blocks-live"
  LOG+=".dhat"
else
  VALGRIND="valgrind --leak-check=full --num-callers=25"
  VALGRIND+=" --suppressions=$DIR/valgrind-memcheck.supp"
fi

if [ $A ]; then
  $VALGRIND $BINARY >$LOG 2>&1
elif [ $O ]; then
  $VALGRIND $BINARY
else
  $VALGRIND --log-file=$LOG $BINARY
fi
