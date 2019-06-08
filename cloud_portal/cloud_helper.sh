#!/usr/bin/env bash

DOCKER_COMPOSE='etc/docker-compose.yml'
SQL='./etc/*.sql'


function build_frontend(){
    ./build_scripts/build.sh
}

function setup_cms(){
    printf "Moving into cloud directory\n\n"
    pushd cloud

    printf "Running db migrations\n\n"
    python manage.py migrate

    printf "Setting up cms structure\n\n"
    python manage.py readstructure

    printf "Filling in content\n\n"
    python manage.py filldata

    popd
}

function setup_db(){
    SQL_COUNT=`ls -1q etc/*.sql | wc -l`
    if [[ ${SQL_COUNT} -eq 1 ]]; then
        printf "Using db from sql file ${SQL}\n"
        mysql -h 0.0.0.0 --port=3306 -uroot < ${SQL}
        printf "${SQL} was copied to mysql db\n\n"
    else
        printf "No sql files found at ${SQL}\n"
        exit 1
    fi
}

function setup_env(){
    printf "Setting up cloud portal locally"
    [[ ! -d "env" ]] && printf "Creating virtualenv named 'env'\n\n" && virtualenv env -p python3.7

    printf "Activating python3.7 env\n\n"
    . ./env/bin/activate

    printf "Installing pip packages for build_scripts and cloud\n\n"
    pip install -r build_scripts/requirements.txt
    pip install -r cloud/requirements.txt
}

function start_docker_containers() {
    if [[ -e ${DOCKER_COMPOSE} ]]; then
        printf "Starting mysql and redis containers\n\n"
        docker-compose -f ${DOCKER_COMPOSE} up -d
        printf "\n\n"
    else
        printf "No docker-compose file found in ./etc\n\n"
        exit 1
    fi
}

function stop_docker_containers() {
    if [[ -e ${DOCKER_COMPOSE} ]]; then
        printf "Stopping mysql and redis containers\n\n"
        docker-compose -f ${DOCKER_COMPOSE} down
    else
        printf "No docker-compose file found in ./etc\n\n"
        exit 1
    fi
}

for command in $@
do
    case "$command" in
        init)
            start_docker_containers
            setup_env
            setup_db
            build_frontend
            setup_cms
            ;;
        build_frontend)
            build_frontend
            ;;

        generate_cms_docs)
            setup_env
            pushd cloud
            python manage.py json_to_table
            popd
            echo 'Generated files are created in ./cloud/cms'
            ;;

        rebuild_frontend)
            build_frontend
            setup_cms
            ;;
        setup_cms)
            . ./env/bin/activate
            setup_cms
            ;;
        setup_db)
            setup_db
            ;;
        setup_env)
            setup_env
            ;;
        start_docker)
            start_docker_containers
            ;;
        stop_docker)
            stop_docker_containers
            ;;
        *)
            echo Usage: cloud_shortcuts '[init|build_frontend|rebuild_frontend|setup_cms|setup_db|setup_env|start_docker|stop_docker]'
            echo 'init - Does everything. Only run this once'
            echo 'build_frontend - Builds the frontend'
            echo 'generate_cms_docs - Creates an html file for each product in cms/cms_structure.json'
            echo 'rebuild_frontend - Rebuilds the frontend and runs readstructre and filldata commands'
            echo 'setup_cms - Fills in the cms. Runs migrate, readstructure and filldata commands'
            echo 'setup_db - Loads local db with sql file in ~/develop/nx_vms/cloud_portal/'
            echo 'start_docker - Starts docker containers used by cloud'
            echo 'stop_docker - Stops docker containers used by cloud'
            ;;
    esac
done