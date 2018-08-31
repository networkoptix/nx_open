#!/bin/sh

htpasswd -cb /etc/nginx/notification.passwd $(echo -ne $NOTIFICATION_SECRET | tr : ' ')

TRAFFIC_RELAY_HOSTS_STR="$(echo $TRAFFIC_RELAY_HOST,$TRAFFIC_RELAY_LOCATIONS | tr , '\n' | awk '{print "https://" $1; print "https://*." $1}' | tr '\n' ' ')"
export TRAFFIC_RELAY_HOSTS_STR

export DOLLAR='$'
envsubst < /etc/nginx/conf.d/nginx.conf.template > /etc/nginx/nginx.conf
nginx -g "daemon off;"
