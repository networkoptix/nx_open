#include "rotation.h"

#include <cmath>

#include <nx/utils/math/fuzzy.h>

namespace {

using nx::client::desktop::Rotation;
static const QList<Rotation> standardRotations =
    QList<Rotation>()
    << Rotation(0)
    << Rotation(90)
    << Rotation(180)
    << Rotation(270);

} // namespace

namespace nx {
namespace client {
namespace desktop {

Rotation::Rotation(qreal degrees):
    m_degrees(degrees)
{
}

bool Rotation::isValid() const
{
    return static_cast<bool>(m_degrees);
}

qreal Rotation::value() const
{
    return m_degrees.value_or(0);
}

QString Rotation::toString() const
{
    return QString::number(value());
}

QList<Rotation> Rotation::standardRotations()
{
    return ::standardRotations;
}

Rotation Rotation::closestStandardRotation(qreal degrees)
{
    degrees = std::fmod(degrees, 360);
    if (degrees < 0)
        degrees += 360;

    auto closest = ::standardRotations.first();
    qreal diff = qAbs(degrees - closest.value());

    for (const auto& rotation: ::standardRotations)
    {
        const auto d = qAbs(degrees - rotation.value());
        if (d < diff)
        {
            diff = d;
            closest = rotation;
        }
    }

    return closest;
}

bool Rotation::operator==(const Rotation& other) const
{
    return isValid() == other.isValid()
        && qFuzzyEquals(value(), other.value());
}

bool Rotation::operator!=(const Rotation& other) const
{
    return !(*this == other);
}

} // namespace desktop
} // namespace client
} // namespace nx
