#include "rotation.h"

#include <cmath>

#include <nx/utils/math/fuzzy.h>

namespace nx::vms::client::desktop {

namespace {

static const Rotation::RotationList kStandardRotations {
    Rotation(0),
    Rotation(90),
    Rotation(180),
    Rotation(270),
};

static const Rotation::RotationList kExtendedRotations =
    []()
    {
        auto result = kStandardRotations;
        result.append(Rotation(-90));
        result.append(Rotation(-180));
        result.append(Rotation(-270));
        result.append(Rotation(-360));
        return result;
    }();

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
    auto closest = kExtendedRotations.first();
    qreal diff = qAbs(degrees - closest.value());

    for (const auto& rotation: kExtendedRotations)
    {
        const auto d = qAbs(degrees - rotation.value());
        if (d < diff)
        {
            diff = d;
            closest = rotation;
        }
    }

    return closest.value() < 0
        ? Rotation(closest.value() + 360)
        : closest;
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
