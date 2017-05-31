#!/usr/local/bin/dumb-init /bin/bash
set -e

function instantiate_config()
{
    export CUSTOMIZATION=$1
    export CLOUD_PORTAL_CONF_DIR=$CLOUD_PORTAL_BASE_CONF_DIR/$customization

    local CLOUD_PORTAL_HOST_var=CLOUD_PORTAL_HOST_$customization
    export CLOUD_PORTAL_HOST=${!CLOUD_PORTAL_HOST_var:-$CLOUD_PORTAL_HOST}

    tmp=$(tempfile)
    envsubst < $CLOUD_PORTAL_CONF_DIR/cloud_portal.yaml > $tmp
    mv $tmp $CLOUD_PORTAL_CONF_DIR/cloud_portal.yaml
}

ORIG_CUSTOMIZATION=$CUSTOMIZATION

for customization in $ALL_CUSTOMIZATIONS
do
    instantiate_config $customization
done

CUSTOMIZATION=$ORIG_CUSTOMIZATION

case "$1" in
    copystatic)
        cp -R /app/app/static /static_volume
        ;;
    web)
        /app/env/bin/python manage.py migrate
        /app/env/bin/python manage.py createcachetable

        find /app/app/static | xargs touch
        exec /app/env/bin/gunicorn cloud.wsgi --capture-output --workers 4 --bind :5000 --log-level=debug
        ;;
    celery)
        rm -f /tmp/*.pid
        exec /app/env/bin/celery worker -A notifications -l info --concurrency=1 --pidfile=/tmp/celery-w1.pid
        ;;
    *)
        echo Usage: cloud_portal '[web|celery|copystatic]'
        ;;
esac
