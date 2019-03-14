#!/bin/sh

htpasswd -cb /etc/nginx/notification.passwd $(echo -ne $NOTIFICATION_SECRET | tr : ' ')

DATA_HOSTS_STR="$(echo $DATA_HOSTS | tr , '\n' | awk '{print "https://" $1}' | tr '\n' ' ')"
export DATA_HOSTS_STR

export DOLLAR='$'
envsubst < /etc/nginx/conf.d/nginx.conf.template > /etc/nginx/nginx.conf
nginx -g "daemon off;"
