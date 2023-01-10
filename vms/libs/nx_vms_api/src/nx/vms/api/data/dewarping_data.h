// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QtMath>
#include <QtCore/QList>

#include <nx/reflect/instrument.h>
#include <nx/vms/api/types/dewarping_types.h>

#include "data_macros.h"

namespace nx::vms::api {
namespace dewarping {

struct NX_VMS_API MediaData
{
    /**%apidoc Whether dewarping is currently enabled. */
    bool enabled = false;

    /**%apidoc  Camera mounting mode. */
    FisheyeCameraMount viewMode = FisheyeCameraMount::ceiling;

    /**%apidoc View correction angle, in degrees. */
    qreal fovRot = 0.0;

    /**%apidoc Normalized position of the circle. */
    qreal xCenter = 0.5;
    qreal yCenter = 0.5;

    /**%apidoc Circle radius in range 0..1 (r/width). */
    qreal radius = 0.5;

    /**%apidoc Horizontal stretch. Value 1.0 means no stretch. */
    qreal hStretch = 1.0;

    /**%apidoc Fisheye lens projestion type. */
    CameraProjection cameraProjection = CameraProjection::equidistant;

    /**%apidoc 360 degree spherical panorama horizon correction 1st angle, in degrees. */
    qreal sphereAlpha = 0.0;

    /**%apidoc 360 degree spherical panorama horizon correction 2st angle, in degrees. */
    qreal sphereBeta = 0.0;

    bool operator==(const MediaData& other) const;

    /**
    * Compatibility-layer function to maintain the old way of (de)serializing. Used in the server
    * sql database and in the pre-2.3 client exported layouts.
    */
    QByteArray toByteArray() const;

    /**
    * Compatibility-layer function to maintain the old way of (de)serializing. Used in the server
    * sql database and in the pre-2.3 client exported layouts.
    */
    static MediaData fromByteArray(const QByteArray& data);

    /** If current settings specify a 360-degree VR dewarping. */
    bool is360VR() const;

    /** If specified projection is a 360-degree VR. */
    static bool is360VR(CameraProjection projection);

    /** If current settings specify a 180-degree fisheye dewarping. */
    bool isFisheye() const;

    /** If specified projection is a 180-degree fisheye. */
    static bool isFisheye(CameraProjection projection);

    /** List of all possible panoFactor values for the resource with this dewarping parameters. */
    const QList<int>& allowedPanoFactorValues() const;

    /** List of all possible panoFactor values for specified fisheye camera mount. */
    static const QList<int>& allowedPanoFactorValues(FisheyeCameraMount mount);
};
#define MediaData_Fields \
    (enabled)(viewMode)(fovRot)(xCenter)(yCenter)(radius)(hStretch)(cameraProjection) \
    (sphereAlpha)(sphereBeta)

NX_VMS_API_DECLARE_STRUCT_EX(MediaData, (ubjson)(json))
NX_REFLECTION_INSTRUMENT(MediaData, MediaData_Fields)

struct NX_VMS_API ViewData
{
    static constexpr qreal kDefaultFov = 70.0 * M_PI / 180.0;

    /**%apidoc Whether dewarping is currently enabled. */
    bool enabled = false;

    /**%apidoc [opt] Pan in radians. */
    qreal xAngle = 0;

    /**%apidoc [opt] Tilt in radians. */
    qreal yAngle = 0;

    /**%apidoc [opt] Field of view in radians. */
    qreal fov = kDefaultFov;

    /**%apidoc [opt] Aspect ratio correction multiplier (1, 2 or 4).*/
    int panoFactor = 1;

    bool operator==(const ViewData& other) const;

    /**
     * Compatibility-layer function to maintain the old way of (de)serializing. Used in the server
     * sql database and in the pre-2.3 client exported layouts.
     */
    QByteArray toByteArray() const;

    /**
     * Compatibility-layer function to maintain the old way of (de)serializing. Used in the server
     * sql database and in the pre-2.3 client exported layouts.
     */
    static ViewData fromByteArray(const QByteArray& data);
};

#define ViewData_Fields (enabled)(xAngle)(yAngle)(fov)(panoFactor)
NX_VMS_API_DECLARE_STRUCT_EX(ViewData, (ubjson)(json)(xml)(csv_record))
NX_REFLECTION_INSTRUMENT(ViewData, ViewData_Fields)

} // namespace dewarping
} // namespace nx::vms::api


// Compatibility-layer functions to maintain old way of (de)serializing in the server sql database.
void NX_VMS_API serialize_field(const nx::vms::api::dewarping::MediaData& data, QVariant* target);
void NX_VMS_API deserialize_field(const QVariant& value, nx::vms::api::dewarping::MediaData* target);
void NX_VMS_API serialize_field(const nx::vms::api::dewarping::ViewData& data, QVariant* target);
void NX_VMS_API deserialize_field(const QVariant& value, nx::vms::api::dewarping::ViewData* target);
