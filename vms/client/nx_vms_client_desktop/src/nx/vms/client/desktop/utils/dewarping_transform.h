// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QPointF>

#include <nx/utils/impl_ptr.h>

class QnAspectRatio;

namespace nx::vms::api::dewarping {

struct MediaData;
struct ViewData;

} // namespace nx::vms::api::dewarping

namespace nx::vms::client::desktop {

/**
 * Helper class that provides the bidirectional mapping between coordinates on an image yielded
 * using non-rectilinear projection and coordinates on a corresponding dewarped image.
 */
class NX_VMS_CLIENT_DESKTOP_API DewarpingTransform
{
public:
    /**
     * @param mediaParams The structure that describes the type of non-rectilinear projection used
     *     to yield source image, as well as its parameters and calibration data.
     * @param itemParams The structure that desctibes parameters used to transform source image to
     *     to the new one with rectilinear or equrectangular projection type.
     * @param frameAspectRatio The aspect ratio of the source image required to obtain effective
     *     pixel aspect ratio. 1:1 AR corresponds to 1:1 PAR for fisheye type projection and 2:1 AR
     *     corresponds to 1:1 PAR for 360 degree equrectangular projection type.
     */
    DewarpingTransform(
        const nx::vms::api::dewarping::MediaData& mediaParams,
        const nx::vms::api::dewarping::ViewData& itemParams,
        const QnAspectRatio& frameAspectRatio);
    ~DewarpingTransform();

    /**
     * Maps the given dewarped image coordinates to the source image coordinates. Normalized
     * coordinates are used both for the given dewarped image point and resulting source image
     * point, which means that (0.0, 0.0) and (1.0, 1.0) values stand for the top left and bottom
     * right image points respectively.
     * @param viewPoint Normalized coordinates of a dewarped image point.
     * @return Normalized coordinates of a source image point, nullopt if point with given
     *     coordinates cannot be mapped to the source image.
     */
    std::optional<QPointF> mapToFrame(const QPointF& viewPoint) const;

    /**
     * Maps the given source image coordinates to the dewarped image coordinates. Normalized
     * coordinates are used both for the given source image point and resulting dewarped image
     * point, which means that (0.0, 0.0) and (1.0, 1.0) values stand for the top left and bottom
     * right image points respectively.
     * @param framePoint Normalized coordinates of a source image point.
     * @return Normalized coordinates of a dewarped image point, nullopt if transformation with
     *     given parameters is impossible.
     */
    std::optional<QPointF> mapToView(const QPointF& framePoint) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
