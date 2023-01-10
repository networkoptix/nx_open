// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/vms/common/ptz/component.h>

struct NX_VMS_COMMON_API QnPtzLimits
{
    using Component = nx::vms::common::ptz::Component;

    // TODO: #dmishin refactor this class
    qreal minPan = 0;
    qreal maxPan = 360;
    qreal minTilt = -90;
    qreal maxTilt = 90;
    qreal minFov = 0; //< TODO: Rename to minZoom.
    qreal maxFov = 360; //< TODO: Rename to maxZoom.
    qreal minRotation = 0;
    qreal maxRotation = 360;
    qreal minFocus = 0;
    qreal maxFocus = 1.0;
    int maxPresetNumber = 0; //< -1 means unlimited

    qreal minPanSpeed = -1.0;
    qreal maxPanSpeed = 1.0;
    qreal minTiltSpeed = -1.0;
    qreal maxTiltSpeed = 1.0;
    qreal minZoomSpeed = -1.0;
    qreal maxZoomSpeed = 1.0;
    qreal minRotationSpeed = -1.0;
    qreal maxRotationSpeed = 1.0;
    qreal minFocusSpeed = -1.0;
    qreal maxFocusSpeed = 1.0;

    bool operator==(const QnPtzLimits& other) const;

    double maxComponentValue(Component component) const;

    double minComponentValue(Component component) const;

    double maxComponentSpeed(Component component) const;

    double minComponentSpeed(Component component) const;

    double componentRange(Component component) const;

    double componentSpeedRange(Component component) const;
};
#define QnPtzLimits_Fields (minPan)(maxPan)(minTilt)(maxTilt)(minFov)(maxFov)\
    (minRotation)(maxRotation)(minFocus)(maxFocus)(maxPresetNumber)\
    (minPanSpeed)(maxPanSpeed)(minTiltSpeed)(maxTiltSpeed)(minZoomSpeed)(maxZoomSpeed)\
    (minRotationSpeed)(maxRotationSpeed)(minFocusSpeed)(maxFocusSpeed)
NX_REFLECTION_INSTRUMENT(QnPtzLimits, QnPtzLimits_Fields)

Q_DECLARE_TYPEINFO(QnPtzLimits, Q_MOVABLE_TYPE);
