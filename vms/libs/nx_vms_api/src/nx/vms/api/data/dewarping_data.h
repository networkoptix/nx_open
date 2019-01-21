#pragma once

#include <QtCore/QtMath>
#include <QtCore/QMetaType>

#include "data.h"

namespace nx::vms::api {

struct NX_VMS_API DewarpingData: Data
{
    static constexpr qreal kDefaultFov = 70.0 * M_PI / 180.0;

    /** Whether dewarping is currently enabled. */
    bool enabled = false;

    /** Pan in radians. */
    qreal xAngle = 0;

    /** Tilt in radians. */
    qreal yAngle = 0;

    /** Fov in radians. */
    qreal fov = kDefaultFov;

    /**
     * Aspect ratio correction?
     * multiplier for 90 degrees of.. // TODO: #vasilenko
     */
    int panoFactor = 1;

    /**
     * Compatibility-layer function to maintain old way of (de)serializing. Used in the server sql
     * database and in the pre-2.3 client exported layouts.
     */
    QByteArray toByteArray() const;

    /**
     * Compatibility-layer function to maintain old way of (de)serializing. Used in the server sql
     * database and in the pre-2.3 client exported layouts.
     */
    static DewarpingData fromByteArray(const QByteArray& data);
};

#define DewarpingData_Fields (enabled)(xAngle)(yAngle)(fov)(panoFactor)

} // namespace nx::vms::api

Q_DECLARE_METATYPE(nx::vms::api::DewarpingData)

// Compatibility-layer functions to maintain old way of (de)serializing in the server sql database.
void NX_VMS_API serialize_field(const nx::vms::api::DewarpingData& data, QVariant* target);
void NX_VMS_API deserialize_field(const QVariant& value, nx::vms::api::DewarpingData* target);
