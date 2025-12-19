// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

Q_MOC_INCLUDE("QtCore/QEasingCurve")

namespace nx::vms::client::core {

/**
 * A helper to calculate auto-scrolling velocity based on edge proximity.
 * If a drag operation is in progress, and the dragged item approaches top or bottom edge,
 * an automatic scroll is desirable, with variable velocity depending on how much the edge
 * is "pushed". For wider functionality a horizontal mode (for left or right edge) is also
 * implemented.
 *
 * Create an instance of this helper with desired parameters, and during a drag operation call
 * its `velocity` method to compute desired auto-scrolling velocity.
 */
class NX_VMS_CLIENT_CORE_API ProximityScrollHelper: public QObject
{
    Q_OBJECT
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation
        NOTIFY orientationChanged)
    Q_PROPERTY(bool clipping READ clipping WRITE setClipping NOTIFY clippingChanged)
    Q_PROPERTY(qreal sensitiveAreaLength READ sensitiveAreaLength WRITE setSensitiveAreaLength
        NOTIFY sensitiveAreaLengthChanged)
    Q_PROPERTY(QEasingCurve sensitivityEasing READ sensitivityEasing WRITE setSensitivityEasing
        NOTIFY sensitivityEasingChanged)
    Q_PROPERTY(qreal baseVelocity READ baseVelocity WRITE setBaseVelocity
        NOTIFY baseVelocityChanged)
    Q_PROPERTY(qreal rapidVelocityFactor READ rapidVelocityFactor WRITE setRapidVelocityFactor
        NOTIFY rapidVelocityFactorChanged)

public:
    explicit ProximityScrollHelper(QObject* parent = nullptr);
    virtual ~ProximityScrollHelper() override;

    /** Horizontal or vertical scroll helper mode. Default: `Qt::Vertical`. */
    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation value);

    /**
     * If `clipping` is true, the velocity field is clipped by the scroll area geometry: outside
     * of it it's always zero.
     * If `clipping` is false, the velocity field is clamped at the maximum by the sensitive edges
     * (e.g. top/bottom with Qt::Vertical orientation), and is not affected by the other edges.
     */
    bool clipping() const;
    void setClipping(bool value);

    /** Width or height (depending on the orientation) of the sensitive area near the edge. */
    qreal sensitiveAreaLength() const;
    void setSensitiveAreaLength(qreal value);

    /** Defines velocity curve when the edge is approached. Default: `QEasingCurve::InCubic`. */
    QEasingCurve sensitivityEasing() const;
    void setSensitivityEasing(const QEasingCurve& value);

    /** Base velocity at the edge, in pixels per millisecond. Default: 3. */
    qreal baseVelocity() const;
    void setBaseVelocity(qreal value);

    /** Rapid velocity multiplier, applied when Shift key is pressed on desktop systems. */
    qreal rapidVelocityFactor() const;
    void setRapidVelocityFactor(qreal value);

    /**
     * The main method to compute scrolling velocity from provided parameters.
     * @param geometry Geometry of the scroll area.
     * @position Pointer position in the same coordinate system as `geometry`.
     * @returns Velocity in pixels per millisecond (positive for scroll down, negative - up).
     */
    Q_INVOKABLE qreal velocity(const QRectF& geometry, const QPointF& position) const;

    static void registerQmlType();

signals:
    void orientationChanged();
    void clippingChanged();
    void sensitiveAreaLengthChanged();
    void sensitivityEasingChanged();
    void baseVelocityChanged();
    void rapidVelocityFactorChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::core
