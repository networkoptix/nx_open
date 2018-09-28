#!/bin/bash

CONFIG_FILE="/boot/config.txt"

touch_config_file()
{   
    if [ ! -f $CONFIG_FILE ]
    then
        touch $CONFIG_FILE
    fi
}

enable_camera()
{
    if grep -q "start_x" $CONFIG_FILE
    then
        sed -i "s/start_x=0/start_x=1/" $CONFIG_FILE
    else
        echo "" >> $CONFIG_FILE
        echo "# Enable camera" >> $CONFIG_FILE
        echo "start_x=1" >> $CONFIG_FILE
    fi
}

set_gpu_memory()
{
    local line=`grep gpu_mem $CONFIG_FILE` 
    if [ "$?" = "0" ]
    then
        local mem=`echo $line | grep -o -E '[0-9]+'`
        if (( $mem < 256 ))
        then
            sed -i "s/\bgpu_mem=$mem\b/gpu_mem=256/" $CONFIG_FILE
        fi        
    else
        echo "" >> $CONFIG_FILE
        echo "# Allocate gpu memory" >> $CONFIG_FILE
        echo "gpu_mem=256" >> $CONFIG_FILE
    fi
}

install_v4l2()
{
    local modulePath="/etc/modules-load.d/bcm2835-v4l2.conf"
    if [ ! -f $modulePath ]
    then
        echo "bcm2835-v4l2" > $modulePath    # create a modprobe file to load the driver at boot
        modprobe bcm2835-v4l2                # manually load the driver this time
    fi
}

v4l2_config()
{
    local file="/etc/rc.local"
    local repeat_command="v4l2-ctl --set-ctrl repeat_sequence_header=1"
    local i_frame_command="v4l2-ctl --set-ctrl h264_i_frame_period=15"

    sed -i "s/\"exit 0\"/\"e0\"/g" $file # replace "exit 0" with "e0" temporarily

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
    touch_config_file
    enable_camera
    set_gpu_memory
    install_v4l2
    v4l2_config
    echo "*** Reboot for changes to take effect ***"
}

main
