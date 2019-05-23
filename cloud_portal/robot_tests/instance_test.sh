#!/bin/bash

INSTANCE=$1

function check_version() {
    echo "Deployment done!"
    while :
    do
        HTTP_STATUS=$(curl -s --insecure -o /dev/null -w '%{http_code}' https://${INSTANCE}.la.hdw.mx/api/ping/)
        if [[ "${HTTP_STATUS}" -eq 200 ]]; then
            echo "Got 200! All done!"
            break
        else
            echo "Got $HTTP_STATUS Not done yet..."
        fi
        sleep 10
    done
}

[ -n "$INSTANCE" ] || { echo "No instance, dude"; exit 1; }

while :
do
    STATUS=$(curl -s --insecure http://depcon.hdw.mx/api/checkInstanceStatus?instance=${INSTANCE} | jq -r .state)
    case "$STATUS" in
        ready)
            check_version
            exit 0
            ;;
        failed)
            echo "Deploy Failed!"
            exit 1
            ;;
        *)
            echo "Got status=$STATUS... Not deployed yet..."
            ;;
    esac
    sleep 10
done