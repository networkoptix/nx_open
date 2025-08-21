// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QVector2D>
#include <QtGui/QVector3D>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/math/math.h>
#include <nx/vms/api/data/device_ptz_model.h>

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

    // Multiplies the corresponding components of two vectors (Hadamard product).
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

    Vector restricted(const nx::vms::api::PtzPositionLimits& limits, LimitsType restrictionType) const;

    static Vector rangeVector(const nx::vms::api::PtzPositionLimits& limits, LimitsType limitsType);

    QString toString() const;
    Vector scaleSpeed(const nx::vms::api::PtzPositionLimits& limits) const;
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

NX_VMS_COMMON_API nx::vms::common::ptz::Vector qBound(
    const nx::vms::common::ptz::Vector& position, const nx::vms::api::PtzPositionLimits& limits);

NX_VMS_COMMON_API nx::vms::common::ptz::Vector linearCombine(
    qreal a,
    const nx::vms::common::ptz::Vector& x,
    qreal b,
    const nx::vms::common::ptz::Vector& y);
