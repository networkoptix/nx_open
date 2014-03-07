#!/bin/sh

RETVAL=0

SERVICE_NAME=mediaserver
CUSTOMIZATION=networkoptix
PREFIX=/usr/local/apps/$CUSTOMIZATION/mediaserver/
BIN_NAME=mediaserver
BIN_PATH=$PREFIX/bin/$BIN_NAME

export LD_LIBRARY_PATH=$PREFIX/lib
export VMS_PLUGIN_DIR=$PREFIX/plugins

start() {
    echo -n "Starting $BIN_NAME..... "
    $BIN_PATH -e --conf-file=$PREFIX/etc/mediaserver.conf --log-level=none 2>/dev/null 1>&2&
    if [ `/bin/pidof $BIN_NAME` ]
    then
        echo "OK"
    fi
}

startquiet() {
    start
}

stop() {
    echo -n "Stopping $BIN_NAME..... "
    /usr/bin/killall $BIN_NAME 1>/dev/null
    while [ `/bin/pidof $BIN_NAME` ]
    do
        /bin/sleep 1
    done
    echo "OK"
}


#
#   Main script
#
case "$1" in
    start)
        start
        ;;
    startquiet)
        start
        ;;
    stop)
        stop
        ;;
    status)
        SERVICE_PID="`/bin/pidof $BIN_NAME`"
        if [ ! -z $SERVICE_PID ]
        then
            echo "$SERVICE_NAME is running with pid $SERVICE_PID"
        else
            echo "$SERVICE_NAME is stopped"
        fi
        ;;
    server_status)
        if [ `/bin/pidof $BIN_NAME` ]
        then
            echo "1"
        fi
        ;;
    restart|reload|force-reload)
        stop
        while [ `/bin/pidof $BIN_NAME` ]
        do
            /bin/sleep 1
        done
        start
        ;;
    *)
        echo "Usage: $0 {start|startquiet|stop|restart|status}"
        exit 1
esac

exit $RETVAL

