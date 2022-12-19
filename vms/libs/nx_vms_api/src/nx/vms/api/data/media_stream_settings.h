// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSize>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/resolution_data.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx::vms::api {

struct NX_VMS_API MediaStreamSettings
{
    static const QString kMpjpegBoundary;
    static constexpr int kDefaultMaxCachedFrames = 1;

    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Format,
        webm,
        mpegts,
        mpjpeg,
        mp4,
        mkv,
        _3gp,
        rtp,
        flv,
        f4v
    );

    /**%apidoc Device id (can be obtained from "id", "physicalId" or "logicalId" field
     * via /rest/v{1-}/devices) or MAC address (not supported for certain Devices).
     */
    QnUuid id;

    /**%apidoc [opt] Stream format. */
    Format format = Format::mkv;

    /**%apidoc [opt] Video quality. */
    StreamQuality quality = StreamQuality::normal;

    /**%apidoc [opt]:string Turn on video transcoding to the specified resolution.
     * Resolution string either may contain width and height (for instance 320x240) or height
     * only (for instance 240p). By default, 640x480 is used.
     */
    std::optional<ResolutionData> resolution = std::nullopt;

    /**%apidoc [opt] If present specifies archive stream start position.
     * Otherwise, LIVE stream is provided.
     */
    std::optional<std::chrono::microseconds> positionUs = std::nullopt;

    /**%apidoc [opt] If present, specifies archive stream end position.
     * It is used only if `positionUs` parameter is present.
     */
    std::optional<std::chrono::microseconds> endPositionUs = std::nullopt;

    /**%apidoc [opt] Rotate item, in degrees. If the parameter is absent, video will be
     * rotated to the default value defined in Device settings dialog.
     * %value 0
     * %value 90
     * %value 180
     * %value 270
     */
    std::optional<int> rotation = std::nullopt;

    /**%apidoc [opt] Drop Late Frames.
     */
    std::optional<int> dropLateFrames = std::nullopt;

    /**%apidoc [opt] Stand Frame Duration. If the parameter is present, video speed is
     * limited by real time.
     */
    bool standFrameDuration = false;

    /**%apidoc [opt] Turn on realtime optimization. It will drop some frames if not enough
     * CPU for realtime transcoding.
     */
    bool realTimeOptimization = false;

    /**%apidoc [opt] Send only audio stream. */
    bool audioOnly = false;

    /**%apidoc [opt] Seek to exact time in the archive by the specified `pos`,
     * otherwise seek to the nearest left to the `pos` keyframe (on timeline). Enabling causes
     * the stream to be transcoded. Disabled by default.
     */
    bool accurateSeek = false;

    /**%apidoc [opt] Open high quality stream if parameter is 0 or low quality stream
     * if parameter is 1. By default Server auto detects preferred stream index based on
     * destination resolution.
     */
    StreamIndex stream = nx::vms::api::StreamIndex::undefined;

    /**%apidoc [opt] Fragment length in seconds. Can be used for both live and archive streams -
     * for archive streams effectively it's another way to specify `endPositionUs`.
     */
    std::optional<std::chrono::seconds> durationS = std::nullopt;

    /**%apidoc [opt] Add signature to exported media data, only mp4 and webm
     * formats are supported.
     */
    bool signature = false;

    /**%apidoc [opt] Use absolute UTC timestamps in exported media data,
     * only mp4 format is supported.
     */
    bool utcTimestamps = false;

    /**%apidoc [opt] Add continuous timestamps in exported media data.
     */
    bool continuousTimestamps = false;

    /**%apidoc [opt] Force to download file in browser instead of displaying it.
     */
    bool download = false;

    static QByteArray getMimeType(const QString& format);
};
#define MediaStreamSettings_Fields (id)(format)(quality)(resolution)(positionUs)(endPositionUs)\
    (rotation)(dropLateFrames)(standFrameDuration)(realTimeOptimization)(audioOnly)(accurateSeek)\
    (stream)(durationS)(signature)(utcTimestamps)(continuousTimestamps)(download)
QN_FUSION_DECLARE_FUNCTIONS(MediaStreamSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(MediaStreamSettings, MediaStreamSettings_Fields)

} // namespace nx::vms::api
