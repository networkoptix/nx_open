#include "vector.h"

#include <cmath>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace core {
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

Vector Vector::restricted(const QnPtzLimits& limits, LimitsType restrictionType) const
{
    if (restrictionType == LimitsType::position)
    {
        return Vector(
            std::clamp(pan, limits.minPan, limits.maxPan),
            std::clamp(tilt, limits.minTilt, limits.maxTilt),
            std::clamp(rotation, limits.minRotation, limits.maxRotation),
            std::clamp(zoom, limits.minFov, limits.maxFov));
    }

    return Vector(
            std::clamp(pan, limits.minPanSpeed, limits.maxPanSpeed),
            std::clamp(tilt, limits.minTiltSpeed, limits.maxTiltSpeed),
            std::clamp(rotation, limits.minRotationSpeed, limits.maxRotationSpeed),
            std::clamp(zoom, limits.minZoomSpeed, limits.maxZoomSpeed));
}

/*static*/ Vector Vector::rangeVector(const QnPtzLimits& limits, LimitsType limitsType)
{
    if (limitsType == LimitsType::position)
    {
        return Vector(
            limits.maxPan - limits.minPan,
            limits.maxTilt - limits.minTilt,
            limits.maxRotation - limits.minRotation,
            limits.maxFov - limits.minFov);
    }

    return Vector(
        limits.maxPanSpeed - limits.minPanSpeed,
        limits.maxTiltSpeed - limits.minTiltSpeed,
        limits.maxRotationSpeed - limits.minRotationSpeed,
        limits.maxZoomSpeed - limits.minZoomSpeed);
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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Vector, (json)(eq), PtzVector_Fields, (optional, true));

} // namespace ptz
} // namespace core
} // namespace nx

QDebug operator<<(QDebug dbg, const nx::core::ptz::Vector& ptzVector)
{
    dbg.nospace() << lm("nx::core::ptz::Vector(pan=%1, tilt=%2, rotation=%3, zoom=%4, focus=%5)")
        .args(
            ptzVector.pan,
            ptzVector.tilt,
            ptzVector.rotation,
            ptzVector.zoom,
            ptzVector.focus);

    return dbg.maybeSpace();
}
