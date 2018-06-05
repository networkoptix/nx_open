#include "ptz_vector.h"

#include <cmath>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace core {
namespace ptz {

PtzVector::PtzVector(double pan, double tilt, double rotation, double zoom):
    pan(pan),
    tilt(tilt),
    rotation(rotation),
    zoom(zoom)
{
}

PtzVector::PtzVector(
    const QPointF& point,
    const std::array<PtzComponent, 2>& components)
{
    setComponent(point.x(), components[0]);
    setComponent(point.y(), components[1]);
}

void PtzVector::setComponent(double value, PtzComponent component)
{
    switch (component)
    {
        case PtzComponent::pan:
            pan = value;
            break;
        case PtzComponent::tilt:
            tilt = value;
            break;
        case PtzComponent::rotation:
            rotation = value;
            break;
        case PtzComponent::zoom:
            zoom = value;
            break;
        default:
            NX_ASSERT(false, lit("Wrong component. We should never be here"));
    }
}

PtzVector PtzVector::operator+(const PtzVector& other) const
{
    return PtzVector(
        pan + other.pan,
        tilt + other.tilt,
        rotation + other.rotation,
        zoom + other.zoom);
}

PtzVector PtzVector::operator-(const PtzVector& other) const
{
    return PtzVector(
        pan - other.pan,
        tilt - other.tilt,
        rotation - other.rotation,
        zoom - other.zoom);
}

PtzVector PtzVector::operator*(const PtzVector& other) const
{
    return PtzVector(
        pan * other.pan,
        tilt * other.tilt,
        rotation * other.rotation,
        zoom * other.zoom);
}

PtzVector PtzVector::operator/(const PtzVector& other) const
{
    return PtzVector(
        pan / other.pan,
        tilt / other.tilt,
        rotation / other.rotation,
        zoom / other.zoom);
}

PtzVector PtzVector::operator/(double scalar) const
{
    return PtzVector(
        pan / scalar,
        tilt / scalar,
        rotation / scalar,
        zoom / scalar);
}

double PtzVector::length() const
{
    return std::sqrt(lengthSquared());
}

double PtzVector::lengthSquared() const
{
    return pan * pan
        + tilt * tilt
        + rotation * rotation
        + zoom * zoom;
}

QVector2D PtzVector::toQVector2D() const
{
    return QVector2D(pan, tilt);
}

bool PtzVector::isNull() const
{
    return qFuzzyIsNull(*this);
}

PtzVector operator*(const PtzVector& ptzVector, double scalar)
{
    return PtzVector(
        ptzVector.pan * scalar,
        ptzVector.tilt * scalar,
        ptzVector.rotation * scalar,
        ptzVector.zoom * scalar);
}

PtzVector operator*(double scalar, const PtzVector& ptzVector)
{
    return ptzVector * scalar;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(PtzVector, (json)(eq), PtzVector_Fields);

} // namespace ptz
} // namespace core
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::core::ptz, PtzComponent,
    (nx::core::ptz::PtzComponent::pan, "pan")
    (nx::core::ptz::PtzComponent::pan, "tilt")
    (nx::core::ptz::PtzComponent::pan, "rotation")
    (nx::core::ptz::PtzComponent::pan, "zoom"));

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::core::ptz, PtzComponents,
    (nx::core::ptz::PtzComponent::pan, "pan")
    (nx::core::ptz::PtzComponent::pan, "tilt")
    (nx::core::ptz::PtzComponent::pan, "rotation")
    (nx::core::ptz::PtzComponent::pan, "zoom"));

QDebug operator<<(QDebug dbg, const nx::core::ptz::PtzVector& ptzVector)
{
    dbg.nospace() << lm("nx::core::ptz::PtzVector(pan=%1, tilt=%2, rotation=%3, zoom=%4")
        .args(
            ptzVector.pan,
            ptzVector.tilt,
            ptzVector.rotation,
            ptzVector.zoom);

    return dbg.maybeSpace();
}
