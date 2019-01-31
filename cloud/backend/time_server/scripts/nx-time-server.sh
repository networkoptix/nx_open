#!/bin/bash
### BEGIN INIT INFO
# Provides:          NX Time Server
# Required-Start:    networking
# Required-Stop:
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: NX Time Server
### END INIT INFO

RETVAL=0

SERVICE_NAME=nx-time-server
PREFIX=/opt/networkoptix/time_server/
BIN_NAME=time_server
BIN_PATH=$PREFIX/bin/$BIN_NAME
HTTP_LOG_LEVEL=none

export LD_LIBRARY_PATH=$PREFIX/lib

start() {
    SERVICE_PID="`/bin/pidof $BIN_NAME`"
    if [ ! -z "$SERVICE_PID" ]
    then
       echo "$SERVICE_NAME is already running with pid $SERVICE_PID"
       return 0
    fi

    echo -n "Starting $BIN_NAME..... "
    $BIN_PATH -e --conf-file=$PREFIX/etc/$BIN_NAME.conf 2>/dev/null 1>&2&

    sleep 1

    if [ `/bin/pidof $BIN_NAME` ]
    then
        echo "OK"
    fi
}

startquiet() {
    start
}

SECONDS_TO_WAIT_BEFORE_KILL_9=30

stop() {
    echo -n "Stopping $BIN_NAME..... "
    /usr/bin/killall $1 $BIN_NAME 1>/dev/null
    local i=0
    while [ "`/bin/pidof $BIN_NAME`" ]
    do
        /bin/sleep 1
        let i++
        if [ $i -gt $SECONDS_TO_WAIT_BEFORE_KILL_9 ]; then
            echo -n "sending 9 signal...    "
            /usr/bin/killall -9 $BIN_NAME 1>/dev/null 2>&1
        fi
    done
    echo "OK"
}

start_watchdog() {
    WATCHDOG_PID="`/bin/ps aux | /bin/grep run_watchdog | /bin/grep -v grep | sed -r "s/ +/ /g" | cut -f 2 -d' ' | sort -r | head -n 1`"
    if [ ! -z "$WATCHDOG_PID" ]; then
        echo "$SERVICE_NAME watchdog is already running with pid $WATCHDOG_PID"
        return 0
    fi

    /bin/sh -c "/etc/init.d/$SERVICE_NAME run_watchdog" 2>/dev/null 1>&2&
    echo "$SERVICE_NAME watchdog has been started"
}

stop_watchdog() {
    WATCHDOG_PID="`/bin/ps aux | /bin/grep run_watchdog | /bin/grep -v grep | sed -r "s/ +/ /g" | cut -f 2 -d' ' | sort -r | head -n 1`"
    if [ -z "$WATCHDOG_PID" ]; then
        echo "$SERVICE_NAME watchdog is not running"
        return 0
    fi

    echo -n "Stopping $SERVICE_NAME watchdog..... "
    kill $WATCHDOG_PID
    sleep 1
    echo "OK"
}

run_watchdog() {
    while true
    do
        start
        sleep 5
    done
}


#
#   Main script
#
case "$1" in
    start)
        start
        start_watchdog
        ;;

    startquiet)
        start
        ;;

    start_console)
        $BIN_PATH -e --conf-file=$PREFIX/etc/$BIN_NAME.conf --msg-log-level=$HTTP_LOG_LEVEL
        ;;

    stop)
        stop_watchdog
        stop
        ;;

    kill)
        stop_watchdog
        stop -9
        ;;

    status)
        SERVICE_PID="`/bin/pidof $BIN_NAME`"
        if [ ! -z $SERVICE_PID ]
        then
            echo "$SERVICE_NAME is running with pid $SERVICE_PID"
            exit 0
        else
            echo "$SERVICE_NAME is stopped"
            exit 1
        fi
        ;;

    server_status)
        if [ `/bin/pidof $BIN_NAME` ]
        then
            echo "1"
        fi
        ;;

    restart|reload|force-reload)
        stop_watchdog
        stop
        while [ `/bin/pidof $BIN_NAME` ]
        do
            /bin/sleep 1
        done
        start
        start_watchdog
        ;;

    start_watchdog)
        start_watchdog
        ;;

    run_watchdog)
        run_watchdog
        ;;

    stop_watchdog)
        stop_watchdog
        ;;

    *)
        echo "Usage: $0 {start|startquiet|stop|restart|status}"
        exit 1
esac

exit $RETVAL
