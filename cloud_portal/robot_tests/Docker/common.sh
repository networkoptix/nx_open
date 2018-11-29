function pack()
{
    echo "Packing $MODULE:$VERSION to a container"
    local COMMON_BUILD_ARGS=(--build-arg VERSION="$VERSION" --build-arg REVISION="$REVISION" --build-arg BUILD_DATE="$BUILD_DATE" --build-arg BUILD_HOST="$BUILD_HOST" --build-arg BUILD_USER="$BUILD_USER")
    local ALL_ARGS=("${COMMON_BUILD_ARGS[@]}" "${BUILD_ARGS[@]}")
    docker build -t $MODULE:$VERSION "${ALL_ARGS[@]}" .
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
        if [ "$func" = "stage_cmake" ]
        then
            args="$1 $2"; shift; n=$((n+2))
        fi

        $func $args
    done
}
