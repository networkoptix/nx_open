// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_display_utils.h"

#include <cmath>
#include <numbers>

#include <QtGui/QPainter>

#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace {

static constexpr auto kHotspotRadius = 16;
static constexpr auto kHotspotPointerAngle = 90.0;
static constexpr auto kHotspotBoundsOffset = 4;

static constexpr QSize kHotspotItemIconSize =
{nx::style::Metrics::kDefaultIconSize, nx::style::Metrics::kDefaultIconSize};

static constexpr auto kHotspotItemOpacity = 0.4;
static constexpr auto kSelectedHotspotItemOpacity = 0.6;
static constexpr auto kSelectedHotspotOutlineWidth = 2.0;

static constexpr auto kHotspotItemColorKey = "dark1";
static constexpr auto kHighlightedHotspotItemColorKey = "blue10";
static constexpr auto kInvalidHotspotItemColorKey = "red_l2";

} // namespace

namespace nx::vms::client::desktop {
namespace camera_hotspots {

using Geometry = nx::vms::client::core::Geometry;
using CameraHotspotData = nx::vms::common::CameraHotspotData;

QPointF hotspotOrigin(const CameraHotspotData& hotspot, const QRectF& rect)
{
    const auto origin = Geometry::subPoint(rect, hotspot.pos);
    return Geometry::bounded(origin, Geometry::eroded(rect, kHotspotRadius + kHotspotBoundsOffset));
}

void setHotspotPositionFromPointInRect(
    const QRectF& sourceRect,
    const QPointF& sourcePoint,
    nx::vms::common::CameraHotspotData& outHotspot)
{
    const auto sourceRectPoint = sourcePoint - sourceRect.topLeft();
    outHotspot.pos = Geometry::bounded(
        Geometry::cwiseDiv(sourceRectPoint, sourceRect.size()),
        QRectF(0, 0, 1, 1));
}

QPainterPath makeHotspotOutline(const QPointF& origin, const QPointF& direction)
{
    QPainterPath hotspotRoundOutline;
    hotspotRoundOutline.addEllipse(origin, kHotspotRadius, kHotspotRadius);

    if (direction.isNull())
        return hotspotRoundOutline;

    const auto radiusDirection = Geometry::normalized(direction) * kHotspotRadius;
    const auto hotspotPointerAngleRad = kHotspotPointerAngle * std::numbers::pi / 180.0;

    const auto pointerTipPoint = origin + radiusDirection / std::sin(hotspotPointerAngleRad * 0.5);

    const auto pointerCcwTangentPoint = origin +
        Geometry::rotated(radiusDirection, QPointF(), -90.0 + kHotspotPointerAngle * 0.5);

    const auto pointerCwTangentPoint = origin +
        Geometry::rotated(radiusDirection, QPointF(), 90.0 - kHotspotPointerAngle * 0.5);

    QPainterPath hotspotPointerOutline;
    hotspotPointerOutline.moveTo(pointerTipPoint);
    hotspotPointerOutline.lineTo(pointerCcwTangentPoint);
    hotspotPointerOutline.lineTo(pointerCwTangentPoint);
    hotspotPointerOutline.lineTo(pointerTipPoint);

    return hotspotRoundOutline.united(hotspotPointerOutline);
}

QPainterPath makeHotspotOutline(
    const nx::vms::common::CameraHotspotData& hotspot,
    const QRectF& rect)
{
    const auto origin = hotspotOrigin(hotspot, rect);
    if (qFuzzyIsNull(Geometry::length(hotspot.direction)))
        return makeHotspotOutline(origin);

    const auto absoluteDirection = Geometry::cwiseMul(hotspot.direction, rect.size());
    return makeHotspotOutline(origin, absoluteDirection);
}

void paintHotspot(
    QPainter* painter,
    const nx::vms::common::CameraHotspotData& hotspot,
    const CameraHotspotDisplayOption& option)
{
    using namespace nx::vms::client::core;

    // Paint hotspot body.
    const auto hotspotOutline = makeHotspotOutline(hotspot, option.rect);

    const QBrush regularBrush(
        option.cameraState == CameraHotspotDisplayOption::CameraState::invalid
            ? colorTheme()->color(kInvalidHotspotItemColorKey)
            : colorTheme()->color(kHotspotItemColorKey));

    const QBrush higlightedBrush(
        option.cameraState == CameraHotspotDisplayOption::CameraState::invalid
            ? colorTheme()->color(kInvalidHotspotItemColorKey)
            : colorTheme()->color(kHighlightedHotspotItemColorKey));

    QBrush brush = regularBrush;
    double opacity = kHotspotItemOpacity;

    if (option.state == CameraHotspotDisplayOption::State::hovered)
    {
        brush = higlightedBrush;
        if (option.cameraState == CameraHotspotDisplayOption::CameraState::invalid)
            opacity = kSelectedHotspotItemOpacity;
    }

    if (option.state == CameraHotspotDisplayOption::State::selected)
    {
        brush = higlightedBrush;
        opacity = kSelectedHotspotItemOpacity;
    }

    painter->setOpacity(opacity);
    painter->fillPath(hotspotOutline, brush);

    // Paint selected item outline.
    if (option.state == CameraHotspotDisplayOption::State::selected)
    {
        painter->setOpacity(1.0);
        QPen selectedOutlinePen(higlightedBrush, kSelectedHotspotOutlineWidth);
        painter->strokePath(hotspotOutline, selectedOutlinePen);
    }

    // Paint pixmap.
    painter->setOpacity(1.0);
    QPixmap pixmap = qnResIconCache->icon(QnResourceIconCache::Camera)
        .pixmap(nx::style::Metrics::kDefaultIconSize, QIcon::Selected);

    if (option.cameraState == CameraHotspotDisplayOption::CameraState::invalid)
        pixmap = qnSkin->colorize(pixmap, colorTheme()->color(kInvalidHotspotItemColorKey));

    if (option.cameraState == CameraHotspotDisplayOption::CameraState::noCamera)
        painter->setOpacity(0.4);

    const auto iconOffset = -Geometry::toPoint(kHotspotItemIconSize) / 2.0 - QPoint(0, -1);
    const auto origin = hotspotOrigin(hotspot, option.rect);
    painter->drawPixmap(origin + iconOffset, pixmap);
}

} // namespace camera_hotspots
} // namespace nx::vms::client::desktop
