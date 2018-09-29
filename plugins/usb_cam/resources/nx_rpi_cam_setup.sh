#!/bin/bash

CONFIG_FILE="/boot/config.txt"
REBOOT=0

checkRunningUnderRoot()
{
    if [ "$(id -u)" != "0" ]; then
        echo "ERROR: $0 should be run under root" >&2
        exit 1
    fi
}

touchConfigurationFile()
{   
    if [ ! -f $CONFIG_FILE ]
    then
        touch $CONFIG_FILE
    fi
}

enableCamera()
{
    local line=`grep "start_x" $CONFIG_FILE`
    if [ ! -z "$line" ]
    then
        local enabled=`echo $line | grep -o -E '[0-9]+'`
        if [ "$enabled" -eq "0" ]
        then
            sed -i "s/start_x=0/start_x=1/" $CONFIG_FILE 
            REBOOT=1
        fi
    else
        echo "" >> $CONFIG_FILE
        echo "# Enable camera" >> $CONFIG_FILE
        echo "start_x=1" >> $CONFIG_FILE
        REBOOT=1
    fi
}

setGPUMemory()
{
    local line=`grep "gpu_mem" $CONFIG_FILE`
    if [ ! -z "$line" ]
    then
        local mem=`echo $line | grep -o -E '[0-9]+'`
        if [ "$mem" -lt "256" ] 
        then
            sed -i "s/\bgpu_mem=$mem\b/gpu_mem=256/" $CONFIG_FILE
            REBOOT=1
        fi        
    else
        echo "" >> $CONFIG_FILE
        echo "# Allocate gpu memory" >> $CONFIG_FILE
        echo "gpu_mem=256" >> $CONFIG_FILE
        REBOOT=1
    fi
}

installV4L2()
{
    local modulePath="/etc/modules-load.d/bcm2835-v4l2.conf"
    if [ ! -f $modulePath ]
    then
        touch $modulePath
    fi

    if ! grep -q "bcm2835-v4l2" $modulePath 
    then
        echo "bcm2835-v4l2" > $modulePath    # create a modprobe file to load the driver at boot
    fi

    if ! lsmod | grep -q bcm2835_v4l2 # lsmod reports driver with an underscore
    then
        modprobe bcm2835_v4l2 # manually load the driver this time
    fi
}

configureV4L2()
{
    local file="/etc/rc.local"
    local repeat_command="v4l2-ctl --set-ctrl repeat_sequence_header=1"
    local i_frame_command="v4l2-ctl --set-ctrl h264_i_frame_period=15"

    sed -i "s/\"exit 0\"/\"e0\"/g" $file # replace "exit 0" (with quotes) with "e0" temporarily

    if ! grep -q "repeat_sequence_header" $file
    then
        sed -i "s/exit 0/$repeat_command # repeat sps pps every I frame\n\nexit 0/" $file
    fi

    if ! grep -q "h264_i_frame_period" $file
    then
        sed -i "s/exit 0/$i_frame_command # set I frame interval\n\nexit 0/" $file
    fi

    sed -i "s/e0/exit 0/g" $file # replace "e0" with "exit 0" from earlier

    eval $repeat_command
    eval $i_frame_command  
}

main()
{
    checkRunningUnderRoot
    touchConfigurationFile
    enableCamera
    setGPUMemory
    installV4L2
    configureV4L2
    
    if [ "$REBOOT" -eq "1" ]
    then
        echo "*** Reboot for changes to take effect ***"
    fi

    exit 0
}

main
