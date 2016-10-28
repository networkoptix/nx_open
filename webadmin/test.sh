#!/bin/bash
PATH=$PATH:/$(pwd)/node_modules/.bin

if [ -z $1 ]; then IP=10.1.5.147; else IP=$1; fi
if [ -z $2 ]; then PORT=7001; else PORT=$2; fi

kill_pids () {
    echo "Killing all $1 instances..."
    PIDS=()
    PIDS=$(ps aux | grep $1 | grep -v grep | awk {'print $2'})
    for PID in $PIDS; do kill -9 $PID; done
}

kill_pids vfb
kill_pids chrome
kill_pids selenium
kill_pids ffmpeg

rm proxy.json
touch proxy.json
echo "{" > proxy.json
echo "    \"proxy_server_host\": \"$IP\"," >> proxy.json
echo "    \"proxy_server_port\": \"$PORT\"" >> proxy.json
echo "}" >> proxy.json

xvfb-run --listen-tcp --server-num 44 --auth-file /tmp/xvfb.auth -s "-ac -screen 0 1280x720x24" grunt test > grunt.log 2>&1 &
if [ -f /tmp/grunt.mp4 ]; then rm -f /tmp/grunt.mp4; fi
tmux new-session -d -s ffmpeg_buffer 'ffmpeg -f x11grab -video_size 1280x720 -i 127.0.0.1:44 -codec:v libx264 -r 12 /tmp/grunt.mp4'