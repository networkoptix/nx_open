// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRectF>
#include <QtCore/QSize>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/rect_as_string.h>
#include <nx/vms/api/data/resolution_data.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/api/types/resource_types.h>

namespace nx::vms::api {

struct NX_VMS_API MediaSettings
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(ValidationResult,
        nullId,
        invalidStreamIndex,
        invalidRotation,
        invalidDewarpingPanofactor,
        invalidZoom,
        isValid
    );

    /**%apidoc[opt]
     *     %value undefined The Server auto-detects the preferred stream index based on the
     *     destination resolution.
     */
    StreamIndex stream = nx::vms::api::StreamIndex::undefined;

    /**%apidoc[opt]
     * If present, specifies the Archive stream start position. Otherwise, the Live stream is
     * provided.
     */
    std::optional<std::chrono::microseconds> positionUs = std::nullopt;

    /**%apidoc[opt]:string
     * The option will be applied only when the stream transcoding is turned on by some of other
     * transcoding options, or because the original stream codec doesn't fit within the Client
     * request. The resolution format is `{width}x{height}`. The string may contain either a width
     * and height (for instance, 320x240) or a height only (for instance, 240p). By default, the
     * default secondary stream resolution is used.
     */
    std::optional<ResolutionData> resolutionWhenTranscoding = std::nullopt;

    /**%apidoc[opt]:string
     * A transcoding option. Resolution is in format `{width}x{height}`. The string may contain
     * either a width and height (for instance, 320x240), or a height only (for instance, 240p).
     */
    std::optional<ResolutionData> resolution = std::nullopt;

    /**%apidoc[opt]
     * A transcoding option. Item rotation, in degrees. If the parameter is `auto`, the video will
     * be rotated to the default value defined in the Device Settings dialog.
     *
     *     %value "auto"
     *     %value "0"
     *     %value "90"
     *     %value "180"
     *     %value "270"
     */
    std::string rotation = "0";

    /**%apidoc
     * A transcoding option. Image dewarping. If the parameter is absent, image dewarping will
     * depend on the value in the Device settings dialog.
     */
    std::optional<bool> dewarping;

    /**%apidoc[opt]
     * A transcoding option. Dewarping pan in radians.
     */
    qreal dewarpingXangle = 0;

    /**%apidoc[opt]
     * A transcoding option. Dewarping tilt in radians.
     */
    qreal dewarpingYangle = 0;

    /**%apidoc[opt]
     * A transcoding option. Dewarping field of view in radians.
     */
    qreal dewarpingFov = dewarping::ViewData::kDefaultFov;

    /**%apidoc[opt]
     * A transcoding option. Dewarping aspect ratio correction multiplier (1, 2 or 4).
     */
    int dewarpingPanofactor = 1;

    /**%apidoc[opt]:string
     * A transcoding option. Zooms the selected image region. The format is
     * `{x},{y},{width}x{height}` with values in range [0..1].
     */
    std::optional<RectAsString> zoom;

    /**%apidoc[opt]
     * A transcoding option. Transcodes a multi-sensor camera into one stream.
     */
    bool panoramic = true;

    ValidationResult validateMediaSettings() const;

    bool isValid() const;

    bool isLiveRequest() const;
};
#define MediaSettings_Fields (stream)(positionUs)(resolution)(resolutionWhenTranscoding)\
    (rotation)(dewarping)(dewarpingXangle)(dewarpingYangle)(dewarpingFov)(dewarpingPanofactor)(zoom)
QN_FUSION_DECLARE_FUNCTIONS(MediaSettings, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(MediaSettings, MediaSettings_Fields)

} // namespace nx::vms::api
