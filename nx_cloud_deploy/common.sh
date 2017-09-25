MODULES=(cloud_db cloud_portal cloud_portal_nginx connection_mediator vms_gateway traffic_relay nxcloud_host_agent)

case $(uname -s) in
    Linux)
        OS=linux
        SED="sed -i.bak"
        ;;
    Darwin)
        OS=mac
        SED="sed -i '.bak'"
        ;;
    *)
        OS=unknown
        SED=unknown
        ;;
esac


function is_dir_by_var()
{
    local varname=$1
    local dirpath=$(eval 'echo $'"$varname")

    [ -z "$dirpath" ] && { echo "Variable \"\$$varname\" is not defined"; return 1; }
    [ ! -d "$dirpath" ] && { echo "Directory \"$dirpath\" pointed by \"\$$varname\" variable does not exist"; return 1; }

    return 0
}

function check_vms_dirs()
{
    is_dir_by_var environment || { echo Exiting..; exit 1; }
    is_dir_by_var NX_VMS_DIR || { echo Exiting..; exit 1; }
}

function pack()
{
    echo "Packing $MODULE:$VERSION to a container"
    local COMMON_BUILD_ARGS=(--build-arg VERSION="$VERSION" --build-arg REVISION="$REVISION" --build-arg BUILD_DATE="$BUILD_DATE" --build-arg BUILD_HOST="$BUILD_HOST" --build-arg BUILD_USER="$BUILD_USER")
    local ALL_ARGS=("${COMMON_BUILD_ARGS[@]}" "${BUILD_ARGS[@]}")
    docker build -t $MODULE:$VERSION "${ALL_ARGS[@]}" .
}

function pushns()
{
    echo "Pushing $MODULE:$VERSION to the private registry"
    docker tag $MODULE:$VERSION la.hdw.mx:5000/$MODULE:$VERSION
    docker push la.hdw.mx:5000/$MODULE:$VERSION
}

function push()
{
    $(aws ecr get-login --no-include-email)

    echo "Pushing $MODULE:$VERSION to the ECR registry"
    docker tag $MODULE:$VERSION 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/$MODULE:$VERSION
    docker push 009544449203.dkr.ecr.us-east-1.amazonaws.com/cloud/$MODULE:$VERSION
}

function clean()
{
    echo "Cleaning image $MODULE:$VERSION"
    docker images -q $MODULE:$VERSION | xargs docker rmi -f
}

function version()
{
    echo $VERSION
}

function main()
{
    local args=""
    local n=1

    # Check if we have docker here
    if ! docker info &> /dev/null; then
        echo 'No docker server found. If you are Evgeny make sure docker-machine is runnning.'
        exit 1
    fi

    numargs=$#
    for ((n=1;n <= numargs; n++))
    do
        func=$1; shift
        args=""

        if [ "$func" = "publish" ]
        then
            args="$1"; shift; n=$((n+1))
        fi

        $func $args
    done
}
