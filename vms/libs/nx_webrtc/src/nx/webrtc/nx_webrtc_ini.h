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

    NX_INI_STRING("42e01f",
        webrtcAlternativeH264ProfileLevelId,
        "The profile-level-id of the H.264 an m-line offers next to the codec it streams, so\n"
        "that a Device may switch to it with no new offer. The default is constrained baseline\n"
        "at level 3.1, which every WebRTC receiver decodes. An empty value leaves the parameter\n"
        "out, which RFC 6184 section 8.1 reads as baseline at level 1.");
};

Ini& ini();
