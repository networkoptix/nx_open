// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QPainterPath>
#include <QtCore/QPointF>

#include <nx/vms/common/resource/camera_hotspots_data.h>

namespace nx::vms::client::desktop {
namespace camera_hotspots {

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
    };
    State state = State::none;

    enum class CameraState
    {
        noCamera,
        valid,
        invalid,
    };
    CameraState cameraState = CameraState::noCamera;

    QRectF rect;
};

void paintHotspot(
    QPainter* painter,
    const nx::vms::common::CameraHotspotData& hotspot,
    const CameraHotspotDisplayOption& option);

} // namespace camera_hotspots
} // namespace nx::vms::client::desktop
