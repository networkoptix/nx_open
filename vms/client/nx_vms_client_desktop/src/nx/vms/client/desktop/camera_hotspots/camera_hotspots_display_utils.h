// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointF>
#include <QtCore/QVariant>
#include <QtGui/QPainterPath>

#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/resource/camera_hotspots_data.h>

namespace nx::vms::client::desktop {
namespace camera_hotspots {

QPointF hotspotOrigin(const QPointF& hotspotRelativePos, const QRectF& rect);
QPointF hotspotOrigin(const nx::vms::common::CameraHotspotData& hotspot, const QRectF& rect);

void setHotspotPositionFromPointInRect(
    const QRectF& sourceRect,
    const QPointF& sourcePoint,
    nx::vms::common::CameraHotspotData& outHotspot);

QPainterPath makeHotspotOutline(
    const QPointF& origin = QPointF(),
    const QPointF& direction = QPointF());

QPainterPath makeHotspotOutline(
    const nx::vms::common::CameraHotspotData& hotspot,
    const QRectF& rect);

struct CameraHotspotDisplayOption
{
    enum class State
    {
        none,
        hovered,
        selected,
        disabled,
    };
    State state = State::none;

    enum class CameraState
    {
        noCamera,
        valid,
        invalid,
    };
    CameraState cameraState = CameraState::noCamera;

    /**
     * Decoration element painted in the center of the hotspot mark. Expected data types are QIcon
     * or any type that can be converted to string. Due to the compact size of the hotspot mark, it
     * makes sense to display short string values only, such as an ordinal index or some special
     * character.
     */
    QVariant decoration;
};

void paintHotspot(
    QPainter* painter,
    const nx::vms::common::CameraHotspotData& hotspot,
    const QPointF& origin,
    const CameraHotspotDisplayOption& option);

} // namespace camera_hotspots
} // namespace nx::vms::client::desktop
