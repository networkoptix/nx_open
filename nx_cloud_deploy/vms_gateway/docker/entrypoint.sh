#!/usr/local/bin/dumb-init /bin/bash

mkdir -p /tmp/core

export SSL_CERTIFICATE_PATH=/opt/networkoptix/vms_gateway/var/cert.pem

tmp=$(tempfile)
envsubst < /opt/networkoptix/vms_gateway/etc/vms_gateway.conf > $tmp
mv $tmp /opt/networkoptix/vms_gateway/etc/vms_gateway.conf

echo $SSL_CERT | base64 -d > /opt/networkoptix/vms_gateway/var/cert.pem
echo $SSL_KEY | base64 -d >> /opt/networkoptix/vms_gateway/var/cert.pem

tail --pid $$ -n0 -F /opt/networkoptix/vms_gateway/var/log/log_file.log &

exec /opt/networkoptix/vms_gateway/bin/vms_gateway -e --general/mediatorEndpoint=$CONNECTION_MEDIATOR_HOST:3345
