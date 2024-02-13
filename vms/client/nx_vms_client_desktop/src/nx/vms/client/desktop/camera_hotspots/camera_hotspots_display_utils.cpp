// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_display_utils.h"

#include <cmath>

#include <QtGui/QPainter>

#include <nx/utils/log/assert.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

using namespace nx::vms::client::desktop;

static constexpr auto kHotspotPointerAngle = 90.0;
static constexpr auto kHotspotBoundsOffset = camera_hotspots::kHotspotRadius / 2;
static constexpr auto kSelectedHotspotOutlineWidth = 1.5;
static constexpr auto kShadowOffset = 1.0;

static constexpr QSize kHotspotItemIconSize =
    {nx::style::Metrics::kDefaultIconSize, nx::style::Metrics::kDefaultIconSize};

static constexpr auto kHotspotBodyOpacity = 0.4;
static constexpr auto kHoveredHotspotBodyOpacity = 0.6;
static constexpr auto kHotspotShadowOpacity = 0.4;

static constexpr auto kHotspotFontSize = 16;

} // namespace

namespace nx::vms::client::desktop {
namespace camera_hotspots {

using Geometry = nx::vms::client::core::Geometry;
using CameraHotspotData = nx::vms::common::CameraHotspotData;

QPointF hotspotOrigin(const CameraHotspotData& hotspot, const QRectF& rect)
{
    return hotspotOrigin(hotspot.pos, rect);
}

QPointF hotspotOrigin(const QPointF& hotspotRelativePos, const QRectF& rect)
{
    const auto origin = Geometry::subPoint(rect, hotspotRelativePos);
    return Geometry::bounded(origin, Geometry::eroded(rect, kHotspotRadius + kHotspotBoundsOffset));
}

QPointF hotspotPointerTip(const QPointF& origin, const QPointF& direction)
{
    if (!NX_ASSERT(!qFuzzyIsNull(direction)))
        return origin;

    return origin + Geometry::normalized(direction) * kHotspotRadius
        / std::sin(qDegreesToRadians(kHotspotPointerAngle * 0.5));
}

QPointF hotspotPointerTip(const common::CameraHotspotData& hotspot, const QRectF& rect)
{
    return hotspotPointerTip(hotspotOrigin(hotspot, rect), hotspot.direction);
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

    const auto pointerCcwTangentPoint = origin +
        Geometry::rotated(radiusDirection, QPointF(), -90.0 + kHotspotPointerAngle * 0.5);

    const auto pointerCwTangentPoint = origin +
        Geometry::rotated(radiusDirection, QPointF(), 90.0 - kHotspotPointerAngle * 0.5);

    const auto pointerTipPoint = hotspotPointerTip(origin, direction);

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

void paintShadow(
    QPainter* painter,
    const CameraHotspotDisplayOption& option,
    const std::function<void()>& paintFunction)
{
    if (option.state == CameraHotspotDisplayOption::State::disabled)
        return;

    QnScopedPainterPenRollback penRollback(painter, {Qt::black});
    QnScopedPainterBrushRollback brushRollback(painter, {Qt::black});
    QnScopedPainterOpacityRollback opacityRollback(painter, kHotspotShadowOpacity);
    QnScopedPainterTransformRollback transformRollback(painter);
    painter->translate(kShadowOffset, kShadowOffset);

    if (NX_ASSERT(paintFunction))
        paintFunction();
}

void paintHotspotBody(
    QPainter* painter,
    const common::CameraHotspotData& hotspot,
    const QPointF& origin,
    const CameraHotspotDisplayOption& option)
{
    using namespace nx::vms::client::core;

    const auto hotspotOutline = makeHotspotOutline(origin, hotspot.direction);

    const QBrush brush(option.isValid
        ? QColor(hotspot.accentColorName)
        : colorTheme()->color("camera.hotspots.invalid"));

    double hotspotBodyOpacity = 0.0;
    switch (option.state)
    {
        case CameraHotspotDisplayOption::State::none:
            hotspotBodyOpacity = kHotspotBodyOpacity;
            break;

        case CameraHotspotDisplayOption::State::hovered:
        case CameraHotspotDisplayOption::State::selected:
            hotspotBodyOpacity = kHoveredHotspotBodyOpacity;
            break;

        case CameraHotspotDisplayOption::State::disabled:
            hotspotBodyOpacity = kHotspotBodyOpacity * nx::style::Hints::kDisabledItemOpacity;
            break;

        default:
            NX_ASSERT(false, "Unexpected hotspot display state");
            break;
    }

    paintShadow(painter, option,
        [&]{ painter->fillPath(hotspotOutline, Qt::black); });

    {
        // Discard part of the shadow overlapping with the semi-transparent hotspot body, so it
        // does not darken its color.
        QnScopedPainterCompositionModeRollback compositionModeRollback(painter,
            QPainter::CompositionMode_Source);

        QnScopedPainterOpacityRollback opacityRollback(painter, hotspotBodyOpacity);
        painter->fillPath(hotspotOutline, brush);
    }

    // Paint selected item outline.
    if (option.state == CameraHotspotDisplayOption::State::selected)
    {
        QnScopedPainterClipPathRollback clipPathRollback(painter);
        painter->setClipPath(hotspotOutline);
        // Actual outline width will be half the width of the pen due clipping.
        painter->strokePath(hotspotOutline, {brush, kSelectedHotspotOutlineWidth * 2.0});
    }
}

void paintHotspotDecoration(
    QPainter* painter,
    const QPointF& origin,
    const CameraHotspotDisplayOption& option)
{
    using namespace nx::vms::client::core;

    if (option.decoration.isNull())
        return;

    QnScopedPainterOpacityRollback opacityRollback(painter,
        option.state == CameraHotspotDisplayOption::State::disabled
            ? nx::style::Hints::kDisabledItemOpacity
            : 1.0);

    // Paint decoration which is icon or text.
    if (option.decoration.canConvert<QString>())
    {
        QFont font;
        font.setPixelSize(kHotspotFontSize);
        QnScopedPainterFontRollback fontRollback(painter, font);
        QnScopedPainterPenRollback penRollback(painter, colorTheme()->color("camera.hotspots.text"));

        QSize size(kHotspotRadius * 2, kHotspotRadius * 2);
        QRect rect(origin.toPoint() - QPoint(kHotspotRadius, kHotspotRadius), size);

        QTextOption textOption;
        textOption.setAlignment(Qt::AlignCenter);

        paintShadow(painter, option,
            [&]{ painter->drawText(rect, option.decoration.toString(), textOption); });

        painter->drawText(rect, option.decoration.toString(), textOption);
    }
    else if (option.decoration.typeId() == QMetaType::QIcon)
    {
        const auto icon = option.decoration.value<QIcon>();
        const auto pixmap = icon.pixmap(nx::style::Metrics::kDefaultIconSize, QIcon::Selected);

        const auto iconOffset = -Geometry::toPoint(kHotspotItemIconSize) / 2.0 - QPoint(0, -1);
        const auto pixmapOrigin = origin + iconOffset;

        paintShadow(painter, option,
            [&]{ painter->drawPixmap(pixmapOrigin, qnSkin->colorize(pixmap, Qt::black)); });

        painter->drawPixmap(pixmapOrigin, option.isValid
            ? pixmap
            : qnSkin->colorize(pixmap, colorTheme()->color("camera.hotspots.invalid")));
    }
}

void paintHotspot(
    QPainter* painter,
    const common::CameraHotspotData& hotspot,
    const QPointF& origin,
    const CameraHotspotDisplayOption& option)
{
    const auto hotspotPixmap =
        paintHotspotPixmap(hotspot, option, painter->device()->devicePixelRatio());

    auto pixmapOrigin = origin;
    pixmapOrigin -= {
        hotspotPixmap.deviceIndependentSize().width() / 2.0,
        hotspotPixmap.deviceIndependentSize().height() / 2.0};

    painter->drawPixmap(pixmapOrigin, hotspotPixmap);
}

QPixmap paintHotspotPixmap(
    const common::CameraHotspotData& hotspot,
    const CameraHotspotDisplayOption& option,
    qreal devicePixelRatio)
{
    static const QSize kHotspotPixmapSize(kHotspotRadius * 3, kHotspotRadius * 3);

    QPixmap hotspotPixmap(kHotspotPixmapSize * devicePixelRatio);
    hotspotPixmap.setDevicePixelRatio(devicePixelRatio);
    hotspotPixmap.fill(Qt::transparent);

    QPainter hotspotPixmapPainter(&hotspotPixmap);
    hotspotPixmapPainter.setRenderHint(QPainter::Antialiasing);

    const auto origin = QRectF({}, kHotspotPixmapSize).center();
    if (option.displayedComponents.testFlag(CameraHotspotDisplayOption::Component::body))
        paintHotspotBody(&hotspotPixmapPainter, hotspot, origin, option);

    if (option.displayedComponents.testFlag(CameraHotspotDisplayOption::Component::decoration))
        paintHotspotDecoration(&hotspotPixmapPainter, origin, option);

    return hotspotPixmap;
}

} // namespace camera_hotspots
} // namespace nx::vms::client::desktop
