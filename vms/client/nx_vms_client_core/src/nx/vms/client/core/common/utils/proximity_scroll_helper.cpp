// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "proximity_scroll_helper.h"

#include <algorithm>

#include <QtCore/QEasingCurve>
#include <QtGui/QGuiApplication>
#include <QtQml/QtQml>

#include <nx/utils/math/fuzzy.h>

namespace nx::vms::client::core {

struct ProximityScrollHelper::Private
{
    Qt::Orientation orientation = Qt::Vertical;
    bool clipping = true;

    qreal sensitiveAreaLength = 48.0;
    QEasingCurve sensitivityEasing = QEasingCurve(QEasingCurve::InCubic);

    qreal baseVelocity = 3.0; //< Pixels per millisecond at edge proximity 1.
    qreal rapidVelocityFactor = 10.0; //< Applied when Shift is pressed on desktop systems.

    /**
     * Defines non-linear relation between cursor proximity to the edge of scroll area and
     * resulting scroll speed. Values in the beginning of the range affect velocity much less
     * (fine positioning) then ones from the end of the range (coarse positioning).
     * @param edgeProximity Value in the [0, 1] range which describes mouse cursor nearness to the
     *  scroll triggering edge of the scroll area.
     * @returns Scrolling velocity in pixels per millisecond.
     */
    qreal velocityByEdgeProximity(qreal edgeProximity) const
    {
        const qreal edgeProximityMultiplier =
            std::max(0.0001, sensitivityEasing.valueForProgress(edgeProximity));

        qreal result = baseVelocity * edgeProximityMultiplier;

        if (QGuiApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier))
            result *= rapidVelocityFactor;

        return result;
    }

    /**
     * @param geometry Geometry of the scroll area.
     * @param cursorPos The cursor position in the same coordinate system as `geometry`.
     * @returns Relative edge proximity where 0.0 is the most distant, 1.0 is the closest.
     */
    qreal leftOrTopEdgeProximity(
        const QRectF& geometry, const QPointF& cursorPos) const
    {
        const QRectF sensitiveArea = orientation == Qt::Vertical
            ? geometry.adjusted(0, 0, 0, -geometry.height() + sensitiveAreaLength)
            : geometry.adjusted(0, 0, -geometry.width() + sensitiveAreaLength, 0);

        if (clipping && !sensitiveArea.contains(cursorPos))
            return 0.0;

        const qreal value = 1.0 - (orientation == Qt::Vertical
            ? (cursorPos.y() - sensitiveArea.top()) / sensitiveAreaLength
            : (cursorPos.x() - sensitiveArea.left()) / sensitiveAreaLength);

        return std::clamp(value, 0.0, 1.0);
    }

    /**
     * @param geometry Geometry of the scroll area.
     * @param cursorPos The cursor position in the same coordinate system as `geometry`.
     * @returns Relative edge proximity where 0.0 is the most distant, 1.0 is the closest.
     */
    qreal rightOrBottomEdgeProximity(
        const QRectF& geometry, const QPointF& cursorPos) const
    {
        const QRectF sensitiveArea = orientation == Qt::Vertical
            ? geometry.adjusted(0, geometry.height() - sensitiveAreaLength, 0, 0)
            : geometry.adjusted(geometry.width() - sensitiveAreaLength, 0, 0, 0);

        if (clipping && !sensitiveArea.contains(cursorPos))
            return 0.0;

        const qreal value = 1.0 - (orientation == Qt::Vertical
            ? (sensitiveArea.bottom() - cursorPos.y()) / sensitiveAreaLength
            : (sensitiveArea.right() - cursorPos.x()) / sensitiveAreaLength);

        return std::clamp(value, 0.0, 1.0);
    }
};

ProximityScrollHelper::ProximityScrollHelper(QObject* parent):
    QObject(parent),
    d(new Private{})
{
}

ProximityScrollHelper::~ProximityScrollHelper()
{
    // Required here for forward-declared scoped pointer destruction.
}

Qt::Orientation ProximityScrollHelper::orientation() const
{
    return d->orientation;
}

void ProximityScrollHelper::setOrientation(Qt::Orientation value)
{
    if (d->orientation == value)
        return;

    d->orientation = value;
    emit orientationChanged();
}

bool ProximityScrollHelper::clipping() const
{
    return d->clipping;
}

void ProximityScrollHelper::setClipping(bool value)
{
    if (d->clipping == value)
        return;

    d->clipping = value;
    emit clippingChanged();
}

qreal ProximityScrollHelper::sensitiveAreaLength() const
{
    return d->sensitiveAreaLength;
}

void ProximityScrollHelper::setSensitiveAreaLength(qreal value)
{
    if (qFuzzyEquals(d->sensitiveAreaLength, value))
        return;

    d->sensitiveAreaLength = value;
    emit sensitiveAreaLengthChanged();
}

QEasingCurve ProximityScrollHelper::sensitivityEasing() const
{
    return d->sensitivityEasing;
}

void ProximityScrollHelper::setSensitivityEasing(const QEasingCurve& value)
{
    if (d->sensitivityEasing == value)
        return;

    d->sensitivityEasing = value;
    emit sensitivityEasingChanged();
}

qreal ProximityScrollHelper::baseVelocity() const
{
    return d->baseVelocity;
}

void ProximityScrollHelper::setBaseVelocity(qreal value)
{
    if (qFuzzyEquals(d->baseVelocity, value))
        return;

    d->baseVelocity = value;
    emit baseVelocityChanged();
}

qreal ProximityScrollHelper::rapidVelocityFactor() const
{
    return d->rapidVelocityFactor;
}

void ProximityScrollHelper::setRapidVelocityFactor(qreal value)
{
    if (qFuzzyEquals(d->rapidVelocityFactor, value))
        return;

    d->rapidVelocityFactor = value;
    emit rapidVelocityFactorChanged();
}

qreal ProximityScrollHelper::velocity(const QRectF& geometry, const QPointF& position) const
{
    const qreal leftOrTopEdgeProximity = d->leftOrTopEdgeProximity(geometry, position);
    const qreal rightOrBottomEdgeProximity = d->rightOrBottomEdgeProximity(geometry, position);

    const qreal proximity = std::max(leftOrTopEdgeProximity, rightOrBottomEdgeProximity);
    if (qFuzzyIsNull(proximity))
        return 0.0;

    const qreal speed = d->velocityByEdgeProximity(proximity);
    return (rightOrBottomEdgeProximity > leftOrTopEdgeProximity) ? speed : -speed;
}

void ProximityScrollHelper::registerQmlType()
{
    qmlRegisterType<ProximityScrollHelper>("nx.vms.client.core", 1, 0, "ProximityScrollHelper");
}

} // namespace nx::vms::client::core
