#!/bin/sh -e

function fatal()
{
    local msg="$1"

    echo $msg 1>&2
    exit 1
}

function checkdir()
{
    local dir="$1"

    [ -d "$dir" ] || fatal "Directory $dir should be mounted"
}

[ $# -eq 0 ] && fatal "Usage: certgen <DOMAIN> [DOMAIN..]"

checkdir /etc/letsencrypt
checkdir /var/lib/letsencrypt
checkdir /var/log/letsencrypt
checkdir /ssl

ARGS=$@

MAIN=$1

for domain in $@
do
    DOMAINS="$DOMAINS -d $domain"
    DOMAINS="$DOMAINS -d *.$domain"
done

certbot certonly -n --dns-route53 --agree-tos --email certgen@enk.me --server https://acme-v02.api.letsencrypt.org/directory $DOMAINS

cat /etc/letsencrypt/live/$MAIN/fullchain.pem /etc/letsencrypt/live/$MAIN/privkey.pem > /ssl/cert.pem
