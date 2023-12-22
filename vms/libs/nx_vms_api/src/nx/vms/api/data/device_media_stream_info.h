// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <vector>

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(StreamTransportType,
    rtsp = 1 << 0,

    /**%apidoc WebM over http. */
    webm = 1 << 1,

    /**%apidoc Apple HTTP Live Streaming. */
    hls = 1 << 2,

    /**%apidoc Motion JPEG over HTTP. */
    mjpeg = 1 << 3,

    webrtc = 1 << 4)
Q_DECLARE_FLAGS(StreamTransportTypes, StreamTransportType)
Q_DECLARE_OPERATORS_FOR_FLAGS(StreamTransportTypes)

struct NX_VMS_API DeviceMediaStreamInfo
{
    // We have to keep compatibility with a previous version. So, this field stays int
    /**%apidoc */
    int encoderIndex = 0;

    /**%apidoc
     * Resolution in format `{width}x{height}` or "*" to notify that any resolution is supported.
     */
    QString resolution; // `ResolutionData` class can't be used because it doesn't support "*"

    /**%apidoc Transport methods that can be used to receive this stream. */
    StreamTransportTypes transports;

    /**%apidoc
     * If `true` this stream is produced by transcoding one of native (having this flag set to
     * `false`) stream.
     */
    bool transcodingRequired = false;
    int codec = 0;
};
#define DeviceMediaStreamInfo_Fields \
    (encoderIndex)(resolution)(transports)(transcodingRequired)(codec)
QN_FUSION_DECLARE_FUNCTIONS(DeviceMediaStreamInfo, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(DeviceMediaStreamInfo, DeviceMediaStreamInfo_Fields)

} // namespace nx::vms::api
