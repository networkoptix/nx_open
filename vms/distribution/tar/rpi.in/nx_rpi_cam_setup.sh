#!/bin/bash

NX_RPI_CONFIG_FILE="/boot/config.txt"
NX_RPI_REBOOT=0
NX_RPI_V4L2_DRIVER_LOADED=0
NX_RPI_CAM_PRESENT=0

checkRunningUnderRoot()
{
    if [ "$(id -u)" != "0" ]; then
        echo "ERROR: $0 should be run under root" >&2
        exit 1
    fi
}

touchConfigurationFile()
{   
    if [ ! -f $NX_RPI_CONFIG_FILE ]
    then
        touch $NX_RPI_CONFIG_FILE
    fi
}

enableRpiCameraInterface()
{
    local line=`grep "start_x" $NX_RPI_CONFIG_FILE`
    if [ ! -z "$line" ]
    then
        local enabled=`echo $line | grep -o -E '[0-9]+'`
        if [ "$enabled" = "0" ]
        then
            sed -i "s/start_x=0/start_x=1/" $NX_RPI_CONFIG_FILE 
            NX_RPI_REBOOT=1
        fi
    else
        echo "" >> $NX_RPI_CONFIG_FILE
        echo "# Enable camera" >> $NX_RPI_CONFIG_FILE
        echo "start_x=1" >> $NX_RPI_CONFIG_FILE
        NX_RPI_REBOOT=1
    fi
}

setGPUMemory()
{
    local line=`grep "gpu_mem" $NX_RPI_CONFIG_FILE`
    if [ ! -z "$line" ]
    then
        local mem=`echo $line | grep -o -E '[0-9]+'`
        if [ "$mem" -lt "256" ] 
        then
            sed -i "s/\bgpu_mem=$mem\b/gpu_mem=256/" $NX_RPI_CONFIG_FILE
            NX_RPI_REBOOT=1
        fi        
    else
        echo "" >> $NX_RPI_CONFIG_FILE
        echo "# Allocate gpu memory" >> $NX_RPI_CONFIG_FILE
        echo "gpu_mem=256" >> $NX_RPI_CONFIG_FILE
        NX_RPI_REBOOT=1
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
        echo "bcm2835-v4l2" > $modulePath # create a modprobe file to load the driver at boot
    fi

    if ! lsmod | grep -q bcm2835_v4l2 # lsmod reports driver with an underscore
    then
        modprobe bcm2835_v4l2 || true # manually load the driver this time, ignore failure
    fi

    if lsmod | grep -q bcm2835_v4l2 # check again to see if it loaded successfully
    then
        NX_RPI_V4L2_DRIVER_LOADED=1
	    echo "v4l2 driver is loaded"
    else
        echo "v4l2_driver is not loaded"
    fi 
}

detectRpiCameraPresence()
{
    # test for /dev/video0 presence before checking if it is rpi cam 
    if [ ! -f "/dev/video0" ]
    then
	    echo "/dev/video0 is not present, mmal camera is not installed"
	    return
    fi

    local mmal=`v4l2-ctl --list-devices | grep mmal`
    if [ ! -z "$mmal" ]
    then
        NX_RPI_CAM_PRESENT=1
	    echo "mmal camera present: $mmal"
    else
	    echo "mmal camera is not present"
    fi
}

configureV4L2()
{
    local file="/etc/rc.local"
    local repeat_command="v4l2-ctl --set-ctrl repeat_sequence_header=1"
    local i_frame_command="v4l2-ctl --set-ctrl h264_i_frame_period=15"

    # Commands need to be inserted before the exit 0 call at the end of /etc/rc.local, but it occurs
    # twice: once at the end (the one we care about) and once in the comments in quotes at the top
    # of the file. Replace the commented one with a unique string temporarily to make stream editing
    # easier. If it doesn't exist in quotes then this won't change anything.
    sed -i "s/\"exit 0\"/\"e0\"/g" $file # replace "exit 0" (with quotes) with "e0" temporarily

    if ! grep -q "repeat_sequence_header" $file
    then
        sed -i "s/exit 0/$repeat_command # repeat sps pps every I frame\n\nexit 0/" $file
    fi

    if ! grep -q "h264_i_frame_period" $file
    then
        sed -i "s/exit 0/$i_frame_command # set I frame interval\n\nexit 0/" $file
    fi

    sed -i "s/\"e0\"/\"exit 0\"/g" $file # replace "e0" with "exit 0" from earlier
    
    if [ "$NX_RPI_CAM_PRESENT" = "1" -a "$NX_RPI_V4L2_DRIVER_LOADED" = "1" ]
    then
	    echo "running rpi setup commands"
        eval $repeat_command || true
        eval $i_frame_command || true
    fi
}

main()
{
    checkRunningUnderRoot
    touchConfigurationFile
    enableRpiCameraInterface
    setGPUMemory
    installV4L2
    detectRpiCameraPresence
    configureV4L2 
    
    if [ "$NX_RPI_REBOOT" = "1" ]
    then
        echo "*** Reboot for changes to take effect ***"
    fi
}

main
