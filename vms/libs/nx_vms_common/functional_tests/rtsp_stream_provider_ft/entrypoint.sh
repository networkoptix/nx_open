#!/bin/bash

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

set -eu

TEST_DIR=/testdir
VIDEO_FILE="$TEST_DIR/test_b_frames.mkv"
FRAME_COUNT=20

run_test()
{
    server_script=$1
    mode=$2
    server_rtp_transport=$3
    port=$4
    expected_checksum=${5-}
    client_rtp_transport=${6-$server_rtp_transport}
    expected_checker_exit_code=${7-0}
    test_name="${mode}_${client_rtp_transport}"
    if [ "$client_rtp_transport" != "$server_rtp_transport" ]; then
        test_name="${test_name}_to_${server_rtp_transport}"
    fi
    raw_output="/tmp/${test_name}_video.h264"
    video_output="/tmp/${test_name}_video.mp4"
    ready_file="/tmp/${test_name}_rtsp_server.ready"

    rm -f "$raw_output" "$video_output" "$ready_file"

    python3 "$TEST_DIR/$server_script" \
        --mode "$mode" \
        --rtp-transport "$server_rtp_transport" \
        --port "$port" \
        --ready-file "$ready_file" \
        --video-file "$VIDEO_FILE" &
    server_pid=$!

    cleanup()
    {
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    }
    trap cleanup RETURN

    for attempt in $(seq 1 20); do
        if [ -f "$ready_file" ]; then
            break
        fi
        sleep 1
    done

    if [ ! -f "$ready_file" ]; then
        echo "$test_name RTSP server did not start."
        return 1
    fi

    checker_exit_code=0
    "$TEST_DIR/rtsp_checker" \
        --url="rtsp://127.0.0.1:${port}/axis-media/media.amp" \
        --auth=none \
        --rtp-transport="$client_rtp_transport" \
        --timeout=5000 \
        --duration=15 \
        --frames="$FRAME_COUNT" \
        --raw-output="$raw_output" \
        --video-output="$video_output" || checker_exit_code=$?

    server_exit_code=0
    wait "$server_pid" || server_exit_code=$?

    if [ "$checker_exit_code" -ne "$expected_checker_exit_code" ]; then
        echo "rtsp_checker exited with code $checker_exit_code for $test_name; " \
            "expected $expected_checker_exit_code."
        return 1
    fi

    if [ "$server_exit_code" -ne 0 ]; then
        echo "$test_name RTSP server failed with exit code $server_exit_code."
        return "$server_exit_code"
    fi

    if [ "$expected_checker_exit_code" -ne 0 ]; then
        return 0
    fi

    if [ ! -s "$raw_output" ]; then
        echo "rtsp_checker did not save raw video for $test_name."
        return 1
    fi

    if [ ! -s "$video_output" ]; then
        echo "rtsp_checker did not save MP4 video for $test_name."
        return 1
    fi

    if ! gst-launch-1.0 --quiet filesrc location="$video_output" ! qtdemux ! fakesink; then
        echo "GStreamer failed to demux MP4 video for $test_name."
        return 1
    fi

    actual_checksum=$(sha256sum "$raw_output" | cut -d " " -f 1)
    echo "$test_name video checksum: $actual_checksum"
    if [ -n "$expected_checksum" ] && [ "$actual_checksum" != "$expected_checksum" ]; then
        echo "Unexpected $test_name video checksum: $actual_checksum"
        return 1
    fi
}

# Run a non-secure RTSP/TCP test to obtain the expected checksum.
run_test gstreamer_rtsp_server.py avp tcp 8553
EXPECTED_VIDEO_SHA256=$(sha256sum /tmp/avp_tcp_video.h264 | cut -d " " -f 1)

# Non secure RTSP tests.
run_test gstreamer_rtsp_server.py avp udp 8554 "$EXPECTED_VIDEO_SHA256"
# Verify automatic fallback from rejected TCP SETUP to UDP.
run_test gstreamer_rtsp_server.py avp udp 8561 "$EXPECTED_VIDEO_SHA256" automatic
# The transport selected for the first track must also be used for subsequent tracks.
run_test rtsp_server.py avp_second_track_transport_rejected tcp 8562 "" automatic 2

# Secure RTSP tests.
run_test gstreamer_rtsp_server.py savp tcp 8555 "$EXPECTED_VIDEO_SHA256"
run_test rtsp_server.py savp udp 8556 "$EXPECTED_VIDEO_SHA256"
run_test rtsp_server.py savp_axis tcp 8557 "$EXPECTED_VIDEO_SHA256"
run_test rtsp_server.py savp_media_level_aes256_cm tcp 8558 "$EXPECTED_VIDEO_SHA256"
run_test rtsp_server.py savp_axis_gcm tcp 8559 "$EXPECTED_VIDEO_SHA256"
run_test rtsp_server.py savp_axis_unparseable tcp 8560 "$EXPECTED_VIDEO_SHA256"
# Verify a single replayed (rejected) SRTP packet over TCP-interleaved does not kill the session.
run_test rtsp_server.py savp_axis_replayed_packet tcp 8563 "$EXPECTED_VIDEO_SHA256"
