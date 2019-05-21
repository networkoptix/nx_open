#!/bin/bash

NX_RPI_CONFIG_FILE="/boot/config.txt"
NX_RPI_REBOOT=0
NX_RPI_V4L2_DRIVER_LOADED=0

# Replaces option in RPi config file.
# Usage: setConfigOption <option> <value> [<condition arg1>..<condition argN>]
#     option - should be valid config.txt option name
#     value - new value for the option
#     conditions - arguments passed to `test`; argument value "@" is substituted with the old 
#         option value, and "@@" - by "@".
setConfigOption()
{
    local -r option="$1" && shift
    local -r new_value="${1//\\/\\\\}" && shift
    local conditions="$*"
    local -r config_lines=$(grep -E "^[[:space:]]*$option[[:space:]]*=" "$NX_RPI_CONFIG_FILE")
    if [[ ! -z $config_line ]]
    then
        config_line=$(echo "$config_lines" |tail -1)
        local -r 
        local -r old_value=$(
            echo "$config_line" |sed "s/[[:space:]]*$option[[:space:]]*=\([^[:space:]]*\)/\1/"
        )

        conditions=$(
            for arg in $conditions
            do
                case "$arg" in
                @) 
                    echo -n "${old_value//\"/\\\"} "
                    ;;
                @@)
                    echo -n "@ "
                    ;;
                *) 
                    echo -n "$arg "
                    ;;
                esac
            done
        )
        conditions=${conditions% }

        eval test ${conditions:-true} &>/dev/null
        if (( $? != 1 )) #< Treat error in conditions as true.
        then
            sed -i "s/^\([[:space:]]*$option[[:space:]]*\)=.*$/\1=${new_value//\//\\\/}/" \
                "$NX_RPI_CONFIG_FILE"
        fi
    else
        echo "" >>"$NX_RPI_CONFIG_FILE"
        echo "$option=$new_value" >>"$NX_RPI_CONFIG_FILE"
    fi
}

checkRunningUnderRoot()
{
    if (( $(id -u) != 0 ))
    then
        echo "ERROR: $0 should be run under root" >&2
        exit 1
    fi
}

touchConfigurationFile()
{   
    if [[ ! -f "$NX_RPI_CONFIG_FILE" ]]
    then
        touch "$NX_RPI_CONFIG_FILE"
    fi
}

enableRpiCameraInterface()
{
    setConfigOption start_x 1
}

setGpuMemory()
{
    setConfigOption gpu_mem 256 @ -lt 256
}

installV4L2()
{
    local -r modulePath="/etc/modules-load.d/bcm2835-v4l2.conf"

    if [[ ! -f $modulePath ]]
    then
        touch "$modulePath"
    fi

    if ! grep -q "bcm2835-v4l2" "$modulePath"
    then
        echo "bcm2835-v4l2" > "$modulePath" #< Create a modprobe file to load the driver at boot.
    fi

    if ! lsmod |grep -q bcm2835_v4l2 #< lsmod reports driver with an underscore.
    then
        modprobe bcm2835_v4l2 || true #< Manually load the driver this time, ignore failure.
    fi

    if lsmod |grep -q bcm2835_v4l2 #< Check again to see if it loaded successfully.
    then
        NX_RPI_V4L2_DRIVER_LOADED=1
	    echo "v4l2 driver is loaded"
    else
        echo "v4l2_driver is not loaded"
    fi 
}

detectRpiCameraPresence()
{
    local -r video_dev_path="/dev/video0"

    # Test for video device presence before checking if it is an RPi cam.
    if [[ ! -c $video_dev_path ]]
    then
	    echo "$video_dev_path is not present, mmal camera is not installed"
	    return
    fi

    local -r mmal=$(v4l2-ctl --list-devices |grep mmal)
    if [[ ! -z $mmal ]]
    then
        NX_RPI_CAM_PRESENT=1
	    echo "mmal camera present: $mmal"
    else
	    echo "mmal camera is not present"
    fi
}

configureV4L2()
{
    # Commands need to be inserted before the exit 0 call at the end of /etc/rc.local, but it 
    # occurs twice: once at the end (the one we care about) and once in the comments in quotes at
    # the top of the file. Replace the commented one with a unique string temporarily to make
    # stream editing easier. If it doesn't exist in quotes then this won't change anything.

    local -r file="/etc/rc.local"
    local -r repeat_command=( v4l2-ctl --set-ctrl repeat_sequence_header=1 )
    local -r i_frame_command=( v4l2-ctl --set-ctrl h264_i_frame_period=15 )

    sed -i "s/\"exit 0\"/\"e0\"/g" "$file" #< Replace "exit 0" (with quotes) with "e0" temporarily.

    if ! grep -q "repeat_sequence_header" "$file"
    then
        sed -i "s/exit 0/${repeat_command[@]} # repeat sps pps every I frame\n\nexit 0/" "$file"
    fi

    if ! grep -q "h264_i_frame_period" "$file"
    then
        sed -i "s/exit 0/${i_frame_command[@]} # set I frame interval\n\nexit 0/" "$file"
    fi

    sed -i "s/\"e0\"/\"exit 0\"/g" "$file" #< Replace "e0" with "exit 0" from earlier.
    
    if (( $NX_RPI_CAM_PRESENT == 1 && $NX_RPI_V4L2_DRIVER_LOADED == 1 ))
    then
	    echo "Running v4l2-ctl commands"
        "${repeat_command[@]}" || true
        "${i_frame_command[@]}" || true
    fi
}

main()
{
    checkRunningUnderRoot
    touchConfigurationFile
    enableRpiCameraInterface
    setGpuMemory
    installV4L2
    detectRpiCameraPresence
    configureV4L2 
}

main "$@"

