// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "rotation.h"

#include <cmath>

#include <nx/utils/math/fuzzy.h>

namespace nx::vms::client::core {

namespace {

static const QList<Rotation> kExtendedRotations {
    Rotation(0),
    Rotation(90),
    Rotation(180),
    Rotation(270),
    Rotation(-90),
    Rotation(-180),
    Rotation(-270),
    Rotation(-360)
};

} // namespace

Rotation::Rotation(qreal degrees):
    m_degrees(degrees)
{
}

Rotation::Rotation(StandardRotation standardRotation):
    m_degrees(static_cast<qreal>(standardRotation))
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

Rotation::RotationList Rotation::standardRotations()
{
    return {
        StandardRotation::rotate0,
        StandardRotation::rotate90,
        StandardRotation::rotate180,
        StandardRotation::rotate270};
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

StandardRotation Rotation::standardRotation(qreal degrees)
{
    return static_cast<StandardRotation>(closestStandardRotation(degrees).value());
}

bool Rotation::operator==(const Rotation& other) const
{
    return qFuzzyEquals(value(), other.value());
}

bool Rotation::operator!=(const Rotation& other) const
{
    return !(*this == other);
}

} // namespace nx::vms::client::core
