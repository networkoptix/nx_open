#!/usr/bin/dumb-init /bin/sh

uid=${FLUENT_UID:-1000}

# check if a old fluent user exists and delete it
cat /etc/passwd | grep fluent >& /dev/null
if [ $? -eq 0 ]; then
    deluser fluent 2> /dev/null
fi

# (re)add the fluent user with $FLUENT_UID
adduser -D -g '' -u ${uid} -h /home/fluent fluent

# chown home and data folder
chown -R fluent /home/fluent
chown -R fluent /fluentd

touch /var/log/es-containers.log.pos
chown fluent /var/log/es-containers.log.pos

su-exec fluent "$@"
