// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>

#include <nx/vms/api/data/dewarping_data.h>

namespace nx::vms::client::core {

struct NX_VMS_CLIENT_CORE_API MediaDewarpingParams: public nx::vms::api::dewarping::MediaData
{
    Q_GADGET
    Q_ENUMS(nx::vms::api::dewarping::FisheyeCameraMount nx::vms::api::dewarping::CameraProjection)

    Q_PROPERTY(bool enabled MEMBER enabled)

    // Fisheye properties.
    Q_PROPERTY(nx::vms::api::dewarping::FisheyeCameraMount viewMode MEMBER viewMode)
    Q_PROPERTY(qreal fovRot MEMBER fovRot)
    Q_PROPERTY(qreal xCenter MEMBER xCenter)
    Q_PROPERTY(qreal yCenter MEMBER yCenter)
    Q_PROPERTY(qreal radius MEMBER radius)
    Q_PROPERTY(qreal hStretch MEMBER hStretch)
    Q_PROPERTY(nx::vms::api::dewarping::CameraProjection cameraProjection MEMBER cameraProjection)

    // 360 panorama properties.
    Q_PROPERTY(qreal sphereAlpha MEMBER sphereAlpha)
    Q_PROPERTY(qreal sphereBeta MEMBER sphereBeta)

    using base_type = nx::vms::api::dewarping::MediaData;

public:
    MediaDewarpingParams() = default;
    MediaDewarpingParams(const MediaDewarpingParams&) = default;
    MediaDewarpingParams(const nx::vms::api::dewarping::MediaData& data): base_type(data) {}
};

} // namespace nx::vms::client::core

Q_DECLARE_METATYPE(nx::vms::client::core::MediaDewarpingParams);
