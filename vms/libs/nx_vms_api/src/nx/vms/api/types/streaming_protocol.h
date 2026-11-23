// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx::vms::api {

using StreamingProtocol = std::string;

namespace streaming_protocol {

/** RTSP over a plain TCP connection. */
inline const StreamingProtocol kRtsp = "rtsp";

/** RTSP over a TLS connection. */
inline const StreamingProtocol kRtsps = "rtsps";

/** Whether the protocol protects the media stream from eavesdropping. */
inline bool isSecure(const StreamingProtocol& protocol)
{
    return protocol == kRtsps;
}

} // namespace streaming_protocol

} // namespace nx::vms::api
