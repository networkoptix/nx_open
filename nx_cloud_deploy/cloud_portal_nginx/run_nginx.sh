#!/bin/sh

htpasswd -cb /etc/nginx/notification.passwd $(echo -ne $NOTIFICATION_SECRET | tr : ' ')

export DOLLAR='$'
envsubst < /etc/nginx/conf.d/nginx.conf.template > /etc/nginx/nginx.conf
nginx -g "daemon off;"
