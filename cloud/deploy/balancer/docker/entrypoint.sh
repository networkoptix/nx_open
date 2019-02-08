#!/bin/bash

for ((n=1;;n++))
do
    domain_var=SSL_DOMAIN_$n
    cert_var=SSL_CERT_$n
    key_var=SSL_KEY_$n

    domain=${!domain_var}
    cert=${!cert_var}
    key=${!key_var}

    [ -z "$domain" ] && break

    echo $cert | base64 -d > /etc/nginx/certs/$domain.crt
    echo $key | base64 -d > /etc/nginx/certs/$domain.key
done

exec nginx "-g" "daemon off;"
