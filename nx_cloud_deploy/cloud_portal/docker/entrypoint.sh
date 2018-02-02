#!/usr/local/bin/dumb-init /bin/bash
set -ex

echo "Running with args: $@"
echo "Environment:"
env
echo "---------------------------"

function update_with_module_configuration()
{
    local config_file=$1
    local configuration=$2

    /usr/local/bin/merge_config.py "$config_file" "$configuration"
}

function instantiate_config()
{
    export CUSTOMIZATION=$1
    export CLOUD_PORTAL_CONF_DIR=$CLOUD_PORTAL_BASE_CONF_DIR/$customization

    local CLOUD_PORTAL_HOST_var=CLOUD_PORTAL_HOST_$customization
    export CLOUD_PORTAL_HOST=${!CLOUD_PORTAL_HOST_var:-$CLOUD_PORTAL_HOST}

    tmp=$(tempfile)
    envsubst < $CLOUD_PORTAL_CONF_DIR/cloud_portal.yaml > $tmp
    mv $tmp $CLOUD_PORTAL_CONF_DIR/cloud_portal.yaml

    if [ -n "$MODULE_CONFIGURATION" ]
    then
        update_with_module_configuration $CLOUD_PORTAL_CONF_DIR/cloud_portal.yaml "$MODULE_CONFIGURATION"
    fi
}

function write_my_cnf()
{
    cat > ~/.my.cnf << EOF
[client]
user = $DB_USER
password = $DB_PASSWORD
host = $DB_HOST
database = $DB_NAME
EOF
}

function instantiate_configs()
{
    ORIG_CUSTOMIZATION=$CUSTOMIZATION

    for customization in $ALL_CUSTOMIZATIONS
    do
        instantiate_config $customization
    done

    CUSTOMIZATION=$ORIG_CUSTOMIZATION
}

for command in $@
do
    case "$command" in
        migratedb)
            write_my_cnf
            echo "CREATE DATABASE IF NOT EXISTS $DB_NAME" | mysql -Dinformation_schema

            yes "yes" | python manage.py migrate
            yes "yes" | python manage.py createcachetable

            python manage.py initdb
            python manage.py readstructure
            python manage.py initbranding
            ;;
        config)
            instantiate_configs
            ;;
        copystatic)
            cp -R /app/app/static /static_volume
            ;;
        web)
            write_my_cnf

            python manage.py filldata

            find /app/app/static | xargs touch
            exec gunicorn cloud.wsgi --capture-output --workers 4 --bind :5000 --log-level=debug --timeout 300
            ;;
        celery)
            write_my_cnf
            rm -f /tmp/*.pid

            python manage.py filldata all

            exec celery worker -A notifications -l info --concurrency=1 --pidfile=/tmp/celery-w1.pid
            ;;
        broadcast_notifications)
            ;;
        *)
            echo Usage: cloud_portal '[web|broadcast_notifications|celery|config|copystatic|migratedb]'
            ;;
    esac
done
