// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointF>
#include <QtCore/QVariant>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>

#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/resource/camera_hotspots_data.h>

namespace nx::vms::client::desktop {
namespace camera_hotspots {

static constexpr auto kHotspotRadius = 16;

QPointF hotspotOrigin(const QPointF& hotspotRelativePos, const QRectF& rect);
QPointF hotspotOrigin(const nx::vms::common::CameraHotspotData& hotspot, const QRectF& rect);

QPointF hotspotPointerTip(const QPointF& origin, const QPointF& direction);
QPointF hotspotPointerTip(const nx::vms::common::CameraHotspotData& hotspot, const QRectF& rect);

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

    enum class TargetState
    {
        noCamera,
        valid,
        invalid,
    };
    TargetState targetState = TargetState::noCamera;

    enum class Component
    {
        none = 0x0,
        body = 0x1,
        decoration = 0x2,
        all = body | decoration
    };
    Q_DECLARE_FLAGS(Components, Component);
    Components displayedComponents = Component::all;

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

QPixmap paintHotspotPixmap(
    const nx::vms::common::CameraHotspotData& hotspot,
    const CameraHotspotDisplayOption& option,
    qreal devicePixelRatio);

} // namespace camera_hotspots
} // namespace nx::vms::client::desktop
