// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vector.h"

#include <cmath>

#include <nx/fusion/model_functions.h>

namespace nx::vms::common {
namespace ptz {

const ComponentVector<2> Vector::kPtComponents = {Component::pan, Component::tilt};

const ComponentVector<3> Vector::kPtrComponents =
    {Component::pan, Component::tilt, Component::rotation};

const ComponentVector<3> Vector::kPtzComponents =
    {Component::pan, Component::tilt, Component::zoom};

const ComponentVector<4> Vector::kPtrzComponents =
    {Component::pan, Component::tilt, Component::rotation, Component::zoom};

const ComponentVector<5> Vector::kPtrzfComponents =
    {Component::pan, Component::tilt, Component::rotation, Component::zoom, Component::focus};

Vector::Vector(double pan, double tilt, double rotation, double zoom, double focus):
    pan(pan),
    tilt(tilt),
    rotation(rotation),
    zoom(zoom),
    focus(focus)
{
}

Vector::Vector(const QPointF& point, const ComponentVector<2>& components)
{
    setComponent(point.x(), components[0]);
    setComponent(point.y(), components[1]);
}

Vector::Vector(const QVector3D& vector, const ComponentVector<3>& components)
{
    setComponent(vector.x(), components[0]);
    setComponent(vector.y(), components[1]);
    setComponent(vector.z(), components[2]);
}

Vector::Vector(const QVector4D& vector, const ComponentVector<4>& components)
{
    setComponent(vector.x(), components[0]);
    setComponent(vector.y(), components[1]);
    setComponent(vector.z(), components[2]);
    setComponent(vector.w(), components[3]);
}

double Vector::component(Component component) const
{
    switch (component)
    {
        case Component::pan: return pan;
        case Component::tilt: return tilt;
        case Component::rotation: return rotation;
        case Component::zoom: return zoom;
        case Component::focus: return focus;
        default:
            NX_ASSERT(false, lit("Wrong component. We should never be here"));
            return qQNaN<double>();
    }
}

void Vector::setComponent(double value, Component component)
{
    switch (component)
    {
        case Component::pan:
            pan = value;
            break;
        case Component::tilt:
            tilt = value;
            break;
        case Component::rotation:
            rotation = value;
            break;
        case Component::zoom:
            zoom = value;
            break;
        case Component::focus:
            focus = value;
            break;
        default:
            NX_ASSERT(false, lit("Wrong component. We should never be here"));
    }
}

bool Vector::operator==(const Vector& other) const
{
    return qFuzzyEquals(*this, other);
}

Vector Vector::operator+(const Vector& other) const
{
    return Vector(
        pan + other.pan,
        tilt + other.tilt,
        rotation + other.rotation,
        zoom + other.zoom,
        focus + other.focus);
}

Vector Vector::operator-(const Vector& other) const
{
    return Vector(
        pan - other.pan,
        tilt - other.tilt,
        rotation - other.rotation,
        zoom - other.zoom,
        focus - other.focus);
}

Vector Vector::operator*(const Vector& other) const
{
    return Vector(
        pan * other.pan,
        tilt * other.tilt,
        rotation * other.rotation,
        zoom * other.zoom,
        focus* other.focus);
}

Vector Vector::operator/(const Vector& other) const
{
    return Vector(
        (other.pan == 0) ? pan / other.pan : qQNaN<double>(),
        (other.tilt == 0) ? tilt / other.tilt : qQNaN<double>(),
        (other.rotation == 0) ? rotation / other.rotation : qQNaN<double>(),
        (other.zoom == 0) ? zoom / other.zoom : qQNaN<double>(),
        (other.focus == 0) ? focus / other.focus : qQNaN<double>());
}

Vector Vector::operator/(double scalar) const
{
    if (scalar == 0)
        return qQNaN<Vector>();

    return Vector(
        pan / scalar,
        tilt / scalar,
        rotation / scalar,
        zoom / scalar,
        focus / scalar);
}

Vector& Vector::operator+=(const Vector& other)
{
    pan += other.pan;
    tilt += other.tilt;
    rotation += other.rotation;
    zoom += other.zoom;
    focus += other.focus;
    return *this;
}

Vector& Vector::operator-=(const Vector& other)
{
    pan -= other.pan;
    tilt -= other.tilt;
    rotation -= other.rotation;
    zoom -= other.zoom;
    focus -= other.focus;
    return *this;
}

Vector& Vector::operator*=(const Vector& other)
{
    pan *= other.pan;
    tilt *= other.tilt;
    rotation *= other.rotation;
    zoom *= other.zoom;
    focus *= other.focus;
    return *this;
}

Vector& Vector::operator*=(double scalar)
{
    pan *= scalar;
    tilt *= scalar;
    rotation *= scalar;
    zoom *= scalar;
    focus *= scalar;
    return *this;
}

Vector& Vector::operator/=(const Vector& other)
{
    (other.pan == 0) ? (pan /= other.pan) : qQNaN<double>();
    (other.tilt == 0) ? (tilt /= other.tilt) : qQNaN<double>();
    (other.rotation == 0) ? (rotation /= other.rotation) : qQNaN<double>();
    (other.zoom == 0) ? (zoom /= other.zoom) : qQNaN<double>();
    (other.focus == 0) ? (focus /= other.focus) : qQNaN<double>();
    return *this;
}

Vector& Vector::operator/=(double scalar)
{
    if (scalar == 0)
    {
        pan = qQNaN<double>();
        tilt = qQNaN<double>();
        rotation /= qQNaN<double>();
        zoom /= qQNaN<double>();
        focus /= qQNaN<double>();
    }
    else
    {
        pan /= scalar;
        tilt /= scalar;
        rotation /= scalar;
        zoom /= scalar;
        focus /= scalar;
    }
    return *this;
}

double Vector::length() const
{
    return std::sqrt(lengthSquared());
}

double Vector::lengthSquared() const
{
    return pan * pan
        + tilt * tilt
        + rotation * rotation
        + zoom * zoom
        + focus * focus;
}

QVector2D Vector::toQVector2D() const
{
    return QVector2D(pan, tilt);
}

bool Vector::isNull() const
{
    return qFuzzyIsNull(*this);
}

bool Vector::isValid() const
{
    return qIsNaN(*this);
}

Vector Vector::restricted(
    const nx::vms::api::PtzPositionLimits& limits, LimitsType restrictionType) const
{
    const auto clamp =
        [](const std::optional<nx::vms::api::MinMaxLimit>& o, const double v)
        {
            return o.transform([=](const nx::vms::api::MinMaxLimit& r) { return r.clamp(v); })
                .value_or(0.0);
        };

    if (restrictionType == LimitsType::position)
    {
        return Vector{
            clamp(limits.pan, pan),
            clamp(limits.tilt, tilt),
            clamp(limits.rotation, rotation),
            clamp(limits.fov, zoom),
            clamp(limits.focus, focus)};
    }
    return Vector{
        clamp(limits.panSpeed, pan),
        clamp(limits.tiltSpeed, tilt),
        clamp(limits.rotationSpeed, rotation),
        clamp(limits.zoomSpeed, zoom),
        clamp(limits.focusSpeed, focus)};
}

/*static*/ Vector Vector::rangeVector(const nx::vms::api::PtzPositionLimits& limits, LimitsType limitsType)
{
    const auto diff =
        [](const std::optional<nx::vms::api::MinMaxLimit>& o)
        {
            return o.transform(&nx::vms::api::MinMaxLimit::range).value_or(0.0);
        };

    if (limitsType == LimitsType::position)
    {
        return Vector{
            diff(limits.pan),
            diff(limits.tilt),
            diff(limits.rotation),
            diff(limits.fov),
            diff(limits.focus)};
    }

    return Vector{
        diff(limits.panSpeed),
        diff(limits.tiltSpeed),
        diff(limits.rotationSpeed),
        diff(limits.zoomSpeed),
        diff(limits.focusSpeed)};
}

QString Vector::toString() const
{
    return nx::format("ptz(%1, %2, %3, r=%4, f=%5)").args(pan, tilt, zoom, rotation, focus);
}

// Scale from [-1, 1] to [min, max] range.
double scaleSpeedComponent(double value, const std::optional<nx::vms::api::MinMaxLimit>& r)
{
    if(!r.has_value())
        return 0.0;
    const auto absolute = (value + 1) / 2;
    return absolute * r->range() + r->min;
}

Vector Vector::scaleSpeed(const nx::vms::api::PtzPositionLimits& limits) const
{
    Vector result;
    result.pan = scaleSpeedComponent(pan, limits.panSpeed);
    result.tilt = scaleSpeedComponent(tilt, limits.tiltSpeed);
    result.rotation = scaleSpeedComponent(rotation, limits.rotationSpeed);
    result.zoom = scaleSpeedComponent(zoom, limits.zoomSpeed);
    result.focus = scaleSpeedComponent(focus, limits.focusSpeed);
    return result;
}

Vector operator*(const Vector& ptzVector, double scalar)
{
    return Vector(
        ptzVector.pan * scalar,
        ptzVector.tilt * scalar,
        ptzVector.rotation * scalar,
        ptzVector.zoom * scalar,
        ptzVector.focus * scalar);
}

Vector operator*(double scalar, const Vector& ptzVector)
{
    return ptzVector * scalar;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Vector, (json), PtzVector_Fields, (optional, true));

} // namespace ptz
} // namespace nx::vms::common

QDebug operator<<(QDebug dbg, const nx::vms::common::ptz::Vector& ptzVector)
{
    dbg.nospace() << nx::format("Vector(pan=%1, tilt=%2, rotation=%3, zoom=%4, focus=%5)")
        .args(
            ptzVector.pan,
            ptzVector.tilt,
            ptzVector.rotation,
            ptzVector.zoom,
            ptzVector.focus);

    return dbg.maybeSpace();
}

qreal calcBestPan(qreal initialPan, nx::vms::api::MinMaxLimit range) noexcept
{
    bool unlimitedPan = false;
    const qreal panRange = range.range();
    if (qFuzzyCompare(panRange, 360) || panRange > 360)
        unlimitedPan = true;

    qreal pan = initialPan;
    if (!unlimitedPan && !range.between(pan))
    {
        /* Round it to the nearest boundary. */
        const qreal panBase = range.min - qMod(range.min, 360.0);
        const qreal panShift = qMod(pan, 360.0);

        qreal bestPan = pan;
        qreal bestDist = std::numeric_limits<qreal>::max();

        pan = panBase - 360.0 + panShift;
        for (int i = 0; i < 3; i++)
        {
            qreal dist{};
            if (pan < range.min)
                dist = range.min - pan;
            else if (pan > range.max)
                dist = pan - range.max;
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
    return pan;
}

nx::vms::common::ptz::Vector qBound(
    const nx::vms::common::ptz::Vector& position, const nx::vms::api::PtzPositionLimits& limits)
{
    const auto clamp =
        [](const std::optional<nx::vms::api::MinMaxLimit>& o, const double v)
        {
            return o.transform([=](const nx::vms::api::MinMaxLimit r) { return r.clamp(v); })
                .value_or(0.0);
        };
    const auto bestPan = limits.pan
        .transform([=](nx::vms::api::MinMaxLimit r) { return calcBestPan(position.pan, r); })
        .value_or(0.0);
    return nx::vms::common::ptz::Vector(
        bestPan,
        clamp(limits.tilt, position.tilt),
        clamp(limits.rotation, position.rotation),
        clamp(limits.fov, position.zoom));
}

nx::vms::common::ptz::Vector linearCombine(
    qreal a,
    const nx::vms::common::ptz::Vector& x,
    qreal b,
    const nx::vms::common::ptz::Vector& y)
{
    return a * x + b * y;
}
