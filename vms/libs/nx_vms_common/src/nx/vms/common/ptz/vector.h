// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QVector2D>
#include <QtGui/QVector3D>

#include <utils/math/math.h>
#include <core/ptz/ptz_limits.h>


#include <nx/utils/math/fuzzy.h>
#include <nx/fusion/model_functions_fwd.h>

#include "component.h"
#include "limits_type.h"

namespace nx::vms::common {
namespace ptz {

struct NX_VMS_COMMON_API Vector
{

public:
    static const ComponentVector<2> kPtComponents;
    static const ComponentVector<3> kPtrComponents;
    static const ComponentVector<3> kPtzComponents;
    static const ComponentVector<4> kPtrzComponents;
    static const ComponentVector<5> kPtrzfComponents;

public:
    Vector() = default;
    Vector(double pan, double tilt, double rotation, double zoom, double focus = 0.0);

    Vector(
        const QPointF& point,
        const ComponentVector<2>& components = {Component::pan, Component::tilt});

    Vector(
        const QVector3D& vector,
        const ComponentVector<3>& components);

    Vector(
        const QVector4D& vector,
        const ComponentVector<4>& components);

    double pan = 0.0;
    double tilt = 0.0;
    double rotation = 0.0;
    double zoom = 0.0;
    double focus = 0.0;

    double component(Component component) const;

    void setComponent(double value, Component component);

    // If you need more freedom degrees fill free to extend this struct.
    bool operator==(const Vector& other) const;

    Vector operator+(const Vector& other) const;

    Vector operator-(const Vector& other) const;

    // Multiplies the corresponding components of two vectors (Hadamar product).
    // Is not the same as cross product.
    Vector operator*(const Vector& other) const;

    Vector operator/(const Vector& other) const;

    Vector operator/(double scalar) const;

    Vector& operator+=(const Vector& other);

    Vector& operator-=(const Vector& other);

    Vector& operator*=(const Vector& other);

    Vector& operator*=(double scalar);

    Vector& operator/=(const Vector& other);

    Vector& operator/=(double scalar);

    double length() const;

    double lengthSquared() const;

    QVector2D toQVector2D() const;

    bool isNull() const;

    bool isValid() const;

    Vector restricted(const QnPtzLimits& limits, LimitsType restrictionType) const;

    static Vector rangeVector(const QnPtzLimits& limits, LimitsType limitsType);

    QString toString() const;
    Vector scaleSpeed(const QnPtzLimits& limits) const;
};

NX_VMS_COMMON_API Vector operator*(const Vector& ptzVector, double scalar);
NX_VMS_COMMON_API  Vector operator*(double scalar, const Vector& ptzVector);

#define PtzVector_Fields (pan)(tilt)(rotation)(zoom)(focus)
QN_FUSION_DECLARE_FUNCTIONS(Vector, (json), NX_VMS_COMMON_API)

} // namespace ptz
} // namespace nx::vms::common

NX_VMS_COMMON_API QDebug operator<<(QDebug dbg, const nx::vms::common::ptz::Vector& ptzVector);

inline bool qFuzzyEquals(
    const nx::vms::common::ptz::Vector& lhs,
    const nx::vms::common::ptz::Vector& rhs)
{
    return ::qFuzzyEquals(lhs.pan, rhs.pan)
        && ::qFuzzyEquals(lhs.tilt, rhs.tilt)
        && ::qFuzzyEquals(lhs.rotation, rhs.rotation)
        && ::qFuzzyEquals(lhs.zoom, rhs.zoom)
        && ::qFuzzyEquals(lhs.focus, rhs.focus);
}

inline bool qFuzzyIsNull(const nx::vms::common::ptz::Vector& ptzVector)
{
    return ::qFuzzyIsNull(ptzVector.pan)
        && ::qFuzzyIsNull(ptzVector.tilt)
        && ::qFuzzyIsNull(ptzVector.rotation)
        && ::qFuzzyIsNull(ptzVector.zoom)
        && ::qFuzzyIsNull(ptzVector.focus);
}

inline bool qIsNaN(const nx::vms::common::ptz::Vector& ptzVector)
{
    return ::qIsNaN(ptzVector.pan)
        || ::qIsNaN(ptzVector.tilt)
        || ::qIsNaN(ptzVector.rotation)
        || ::qIsNaN(ptzVector.zoom)
        || ::qIsNaN(ptzVector.focus);
}

template<>
inline nx::vms::common::ptz::Vector qQNaN<nx::vms::common::ptz::Vector>()
{
    static const double kNan = ::qQNaN<double>();
    return nx::vms::common::ptz::Vector(kNan, kNan, kNan, kNan);
};

template<>
inline nx::vms::common::ptz::Vector qSNaN<nx::vms::common::ptz::Vector>()
{
    static const double kNan = ::qSNaN<double>();
    return nx::vms::common::ptz::Vector(kNan, kNan, kNan, kNan);
}

inline nx::vms::common::ptz::Vector qBound(
    const nx::vms::common::ptz::Vector& position,
    const QnPtzLimits& limits)
{
    bool unlimitedPan = false;
    const qreal panRange = (limits.maxPan - limits.minPan);
    if (qFuzzyCompare(panRange, 360) || panRange > 360)
        unlimitedPan = true;

    qreal pan = position.pan;
    if (!unlimitedPan && !qBetween(limits.minPan, pan, limits.maxPan))
    {
        /* Round it to the nearest boundary. */
        qreal panBase = limits.minPan - qMod(limits.minPan, 360.0);
        qreal panShift = qMod(pan, 360.0);

        qreal bestPan = pan;
        qreal bestDist = std::numeric_limits<qreal>::max();

        pan = panBase - 360.0 + panShift;
        for (int i = 0; i < 3; i++)
        {
            qreal dist;
            if (pan < limits.minPan)
                dist = limits.minPan - pan;
            else if (pan > limits.maxPan)
                dist = pan - limits.maxPan;
            else
                dist = 0.0;

            if (dist < bestDist)
            {
                bestDist = dist;
                bestPan = pan;
            }

            pan += 360.0;
        }

        pan = bestPan;
    }

    return nx::vms::common::ptz::Vector(
        pan,
        qBound((float) limits.minTilt, (float) position.tilt, (float) limits.maxTilt),
        qBound((float) limits.minRotation, (float) position.rotation, (float) limits.maxRotation),
        qBound((float) limits.minFov, (float) position.zoom, (float) limits.maxFov));
}

NX_VMS_COMMON_API nx::vms::common::ptz::Vector linearCombine(
    qreal a,
    const nx::vms::common::ptz::Vector& x,
    qreal b,
    const nx::vms::common::ptz::Vector& y);
