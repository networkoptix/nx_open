#include "rotation.h"

#include <cmath>

#include <nx/utils/math/fuzzy.h>

namespace nx::vms::client::desktop {

namespace {

static const QList<Rotation> kStandardRotations{
    Rotation(0),
    Rotation(90),
    Rotation(180),
    Rotation(270)
};

} // namespace

Rotation::Rotation(qreal degrees):
    m_degrees(degrees)
{
}

qreal Rotation::value() const
{
    return m_degrees;
}

QString Rotation::toString() const
{
    return QString::number(value());
}

QList<Rotation> Rotation::standardRotations()
{
    return kStandardRotations;
}

Rotation Rotation::closestStandardRotation(qreal degrees)
{
    degrees = std::fmod(degrees, 360);
    if (degrees < 0)
        degrees += 360;

    auto closest = kStandardRotations.first();
    qreal diff = qAbs(degrees - closest.value());

    for (const auto& rotation: kStandardRotations)
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
    return qFuzzyEquals(value(), other.value());
}

bool Rotation::operator!=(const Rotation& other) const
{
    return !(*this == other);
}

} // namespace nx::vms::client::desktop
