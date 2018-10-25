#pragma once

#include <QtGui/QVector2D>
#include <QtGui/QVector3D>

#include <utils/math/math.h>
#include <core/ptz/ptz_limits.h>

#include <nx/core/ptz/component.h>
#include <nx/core/ptz/limits_type.h>

#include <nx/utils/math/fuzzy.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace core {
namespace ptz {

struct Vector
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

Vector operator*(const Vector& ptzVector, double scalar);
Vector operator*(double scalar, const Vector& ptzVector);

#define PtzVector_Fields (pan)(tilt)(rotation)(zoom)(focus)
QN_FUSION_DECLARE_FUNCTIONS(Vector, (json))

} // namespace ptz
} // namespace core
} // namespace nx

Q_DECLARE_METATYPE(nx::core::ptz::Vector);

QDebug operator<<(QDebug dbg, const nx::core::ptz::Vector& ptzVector);

inline bool qFuzzyEquals(const nx::core::ptz::Vector& lhs, const nx::core::ptz::Vector& rhs)
{
    return ::qFuzzyEquals(lhs.pan, rhs.pan)
        && ::qFuzzyEquals(lhs.tilt, rhs.tilt)
        && ::qFuzzyEquals(lhs.rotation, rhs.rotation)
        && ::qFuzzyEquals(lhs.zoom, rhs.zoom)
        && ::qFuzzyEquals(lhs.focus, rhs.focus);
}

inline bool qFuzzyIsNull(const nx::core::ptz::Vector& ptzVector)
{
    return ::qFuzzyIsNull(ptzVector.pan)
        && ::qFuzzyIsNull(ptzVector.tilt)
        && ::qFuzzyIsNull(ptzVector.rotation)
        && ::qFuzzyIsNull(ptzVector.zoom)
        && ::qFuzzyIsNull(ptzVector.focus);
}

inline bool qIsNaN(const nx::core::ptz::Vector& ptzVector)
{
    return ::qIsNaN(ptzVector.pan)
        || ::qIsNaN(ptzVector.tilt)
        || ::qIsNaN(ptzVector.rotation)
        || ::qIsNaN(ptzVector.zoom)
        || ::qIsNaN(ptzVector.focus);
}

template<>
inline nx::core::ptz::Vector qQNaN<nx::core::ptz::Vector>()
{
    static const double kNan = ::qQNaN<double>();
    return nx::core::ptz::Vector(kNan, kNan, kNan, kNan);
};

template<>
inline nx::core::ptz::Vector qSNaN<nx::core::ptz::Vector>()
{
    static const double kNan = ::qSNaN<double>();
    return nx::core::ptz::Vector(kNan, kNan, kNan, kNan);
}

inline nx::core::ptz::Vector qBound(
    const nx::core::ptz::Vector& position,
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

    return nx::core::ptz::Vector(
        pan,
        qBound<float>(limits.minTilt, position.tilt, limits.maxTilt),
        qBound<float>(limits.minRotation, position.rotation, limits.maxRotation),
        qBound<float>(limits.minFov, position.zoom, limits.maxFov));
}

nx::core::ptz::Vector linearCombine(
    qreal a,
    const nx::core::ptz::Vector& x,
    qreal b,
    const nx::core::ptz::Vector& y);

