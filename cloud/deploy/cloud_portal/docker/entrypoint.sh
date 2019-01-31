#!/usr/local/bin/dumb-init /bin/bash
set -ex

echo "Running with args: $@"
echo "Environment:"
env
echo "---------------------------"

PORTAL_WORKERS=${PORTAL_WORKERS:-1}

function update_with_module_configuration()
{
    local config_file=$1
    local configuration=$2

    echo "$configuration" | /usr/local/bin/merge_config.py "$config_file" 
}

function instantiate_config()
{
    export CUSTOMIZATION=$1
    export CLOUD_PORTAL_CONF_DIR=$CLOUD_PORTAL_BASE_CONF_DIR/$customization
    mkdir --parents $CLOUD_PORTAL_CONF_DIR

    local CLOUD_PORTAL_CONF_TEMPLATE=$CLOUD_PORTAL_BASE_CONF_DIR/_source/cloud_portal.yaml
    local CLOUD_PORTAL_CONF=$CLOUD_PORTAL_CONF_DIR/cloud_portal.yaml
    local CLOUD_PORTAL_LOCK=${CLOUD_PORTAL_CONF}.lock

    local CLOUD_PORTAL_HOST_var=CLOUD_PORTAL_HOST_$customization
    export CLOUD_PORTAL_HOST=${!CLOUD_PORTAL_HOST_var:-$CLOUD_PORTAL_HOST}

    (
        flock -n 9 || exit 1
        tmp=$(tempfile)

        envsubst < $CLOUD_PORTAL_CONF_TEMPLATE > $tmp
        mv $tmp $CLOUD_PORTAL_CONF

        if [ -n "$MODULE_CONFIGURATION" ]
        then
            update_with_module_configuration $CLOUD_PORTAL_CONF "$MODULE_CONFIGURATION"
        fi

        rm $CLOUD_PORTAL_LOCK
    ) 9> $CLOUD_PORTAL_LOCK

}

function write_my_cnf()
{
    cat > ~/.my.cnf << EOF
[client]
user = $DB_USER
password = $DB_PASSWORD
host = $DB_HOST
port = $DB_PORT
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
            python manage.py readstructure
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
            python manage.py filldata --preview=True &

            find /app/app/static | xargs touch
            exec gunicorn cloud.wsgi --capture-output --workers ${PORTAL_WORKERS} --bind :5000 --log-level=debug --timeout 300
            ;;
        celery)
            write_my_cnf
            rm -f /tmp/*.pid

            echo $'\n'Notifications started: Version $VERSION$'\n'
            exec celery worker -A notifications -l debug --concurrency=1 --pidfile=/tmp/celery-w1.pid
            ;;
        broadcast_notifications)
            write_my_cnf
            rm -f /tmp/*.pid

            echo $'\n'Broadcast notifications started: Version $VERSION$'\n'
            exec celery worker -Q broadcast-notifications -A notifications -l info -B --concurrency=1 --pidfile=/tmp/celery-w1.pid
            ;;
        *)
            echo Usage: cloud_portal '[web|broadcast_notifications|celery|config|copystatic|migratedb]'
            ;;
    esac
done
