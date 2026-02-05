// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/kit/ini_config.h>
#include <nx/network/ssl/certificate.h>

struct Ini: public nx::kit::IniConfig
{
    Ini(): IniConfig("nx_webrtc.ini") { reload(); }

    NX_INI_STRING("", turnServer,
        "Overrides the current Server's TURN server used for WebRTC relaying. Format:\n"
        "[login:password@]turn.example.com");

    NX_INI_STRING("", webrtcIceCandidates,
        "Force to use some types of Ice Candidates during WebRTC connectivity.\n"
        "By default, use all possible types:\n"
        "udp,tcp,srflx,relay");

    NX_INI_FLAG(true, webrtcSrtpTransportEnableH265, "Enable support of H.265 for native SRTP transport.");
    NX_INI_FLAG(true, webrtcFixH264ProfileConstraints, "Overwrite profile level ID in the SDP to"
        "declare base and main profiles constrained, this helps streams be acceptable to Safari.");

};

Ini& ini();
