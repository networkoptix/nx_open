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
    if [[ ! -z $config_lines ]]
    then
        local config_line=$(echo "$config_lines" |tail -1)
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
    echo "Ensuring that MMAL camera is enabled..."
    setConfigOption start_x 1
}

setGpuMemory()
{
    setConfigOption gpu_mem 256 @ -lt 256
}

installV4L2()
{
    local -r modulePath="/etc/modules-load.d/bcm2835-v4l2.conf"

    if ! grep -q "bcm2835-v4l2" "$modulePath"
    then
        echo "Enabling bcm2835-v4l2 module..."
        echo "bcm2835-v4l2" > "$modulePath" #< Create a modprobe file to load the driver at boot.
    fi
}

configureV4L2()
{
    local -r file="/etc/rc.local"

    if ! grep -q "repeat_sequence_header" "$file"
    then
        # Put the v4l2-ctl command above `exit 0` line. This command is needed to repeat PPS and
        # SPS for every I-frame.
        sed -i "/exit 0$/i v4l2-ctl --set-ctrl repeat_sequence_header=1" "$file"
    fi

    if ! grep -q "h264_i_frame_period" "$file"
    then
        # Put the v4l2-ctl command above `exit 0` line. This command is needed for setting the
        # I-frame period equal to 15.
        sed -i "/exit 0$/i v4l2-ctl --set-ctrl h264_i_frame_period=15" "$file"
    fi
}

main()
{
    checkRunningUnderRoot
    touchConfigurationFile
    enableRpiCameraInterface
    setGpuMemory
    installV4L2
    configureV4L2 
}

main "$@"

