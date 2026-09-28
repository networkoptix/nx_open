#!/usr/bin/python3

## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import argparse

import gi

gi.require_version("Gst", "1.0")
gi.require_version("GstRtsp", "1.0")
gi.require_version("GstRtspServer", "1.0")

from gi.repository import GLib, Gst, GstRtsp, GstRtspServer


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--ready-file", required=True)
    parser.add_argument("--mode", choices=("avp", "savp"), required=True)
    parser.add_argument("--rtp-transport", choices=("tcp", "udp"), required=True)
    parser.add_argument("--video-file", required=True)
    args = parser.parse_args()

    Gst.init(None)
    loop = GLib.MainLoop()

    server = GstRtspServer.RTSPServer.new()
    server.set_address("127.0.0.1")
    server.set_service(str(args.port))

    factory = GstRtspServer.RTSPMediaFactory.new()
    factory.set_launch(
        f'( filesrc location="{args.video_file}" ! matroskademux ! '
        "h264parse config-interval=-1 ! "
        "rtph264pay name=pay0 pt=96 config-interval=1 )")
    factory.set_profiles(
        GstRtsp.RTSPProfile.SAVP if args.mode == "savp" else GstRtsp.RTSPProfile.AVP)
    factory.set_protocols(
        GstRtsp.RTSPLowerTrans.TCP
        if args.rtp_transport == "tcp"
        else GstRtsp.RTSPLowerTrans.UDP)

    server.get_mount_points().add_factory("/axis-media/media.amp", factory)

    def on_client_connected(_server, client):
        client.connect("closed", lambda _client: loop.quit())

    server.connect("client-connected", on_client_connected)
    if server.attach(None) == 0:
        raise RuntimeError("Failed to attach GStreamer RTSP server")

    with open(args.ready_file, "w", encoding="ascii") as ready:
        ready.write("ready\n")

    loop.run()


if __name__ == "__main__":
    main()
