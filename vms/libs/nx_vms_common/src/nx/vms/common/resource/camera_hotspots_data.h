// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QPointF>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::common {

/** Data structure that describe single camera hotspot item. */
struct NX_VMS_COMMON_API CameraHotspotData
{
    /**
     * The Id of the camera or layout resource that the hotspot refers to.
     */
    QnUuid targetResourceId;

    /**
     * Hotspot item relative position on the camera item. (0.0, 0.0) and (1.0, 1.0) values stand
     * for the camera item rectangle top left and bottom right corners respectively. Position is
     * determined for the camera frame without any rotation, zoom or dewarping applied.
     */
    QPointF pos;

    /**
     * Hotspot item direction vector. Hotspot item will have pointed appearance in the case of
     * non-zero length direction vector. Vector coordinates are in the relative units, thus, for
     * example, (1.0, 1.0) value means that hotspot item will be pointed in the direction that is
     * collinear to the top-left to bottom-right camera item diagonal vector.
     */
    QPointF direction;

    /**
     * Optional user defined hotspot name.
     */
    QString name;

    /**
     * Optional hotspot accent color represented in the hexadecimal "#RRGGBB" format.
     */
    QString accentColorName;

    /**
     * @return True if cameraId is not null and both X and Y coordinates of the pos are within the
     *     [0.0, 1.0] range. There is no check that cameraId belongs to the actual camera resource
     *     that can be used as hotspot destination.
     */
    bool isValid() const;

    /**
     * @return True if described hotspot item is directional one, i.e direction field is non-zero
     *     vector.
     */
    bool hasDirection() const;

    /**
     * Normalize direction vector and leave either escaped and valid accentColorName or
     * the empty one.
     */
    void fixupData();

    bool operator==(const CameraHotspotData& other) const;
};

NX_REFLECTION_INSTRUMENT(CameraHotspotData, (targetResourceId)(pos)(direction)(name)(accentColorName))

using CameraHotspotDataList = std::vector<CameraHotspotData>;

} // namespace nx::vms::common
