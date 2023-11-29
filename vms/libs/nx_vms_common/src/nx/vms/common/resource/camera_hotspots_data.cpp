// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_hotspots_data.h"

#include <cmath>

#include <nx/utils/math/fuzzy.h>

namespace {

bool isValidColorName(const QString& colorName)
{
    if (!colorName.startsWith("#") || colorName.size() != 7)
        return false;

    bool ok;
    colorName.right(6).toInt(&ok, 16);
    return ok;
}

QString fixupColorName(const QString& colorName)
{
    if (isValidColorName(colorName.trimmed()))
        return colorName.trimmed().toLower();

    return QString();
}

QPointF fixupDirection(const QPointF& direction)
{
    if (qFuzzyIsNull(direction))
        return {};

    return direction / std::sqrt(QPointF::dotProduct(direction, direction)); //< Normalize length.
}

} // namespace

namespace nx::vms::common {

bool CameraHotspotData::isValid() const
{
    return !targetResourceId.isNull() && QRectF(0, 0, 1, 1).contains(pos);
}

bool CameraHotspotData::hasDirection() const
{
    return !qFuzzyIsNull(direction);
}

void CameraHotspotData::fixupData()
{
    direction = fixupDirection(direction);
    accentColorName = fixupColorName(accentColorName);
}

bool CameraHotspotData::operator==(const CameraHotspotData& other) const
{
    return targetResourceId == other.targetResourceId
        && qFuzzyEquals(pos, other.pos)
        && qFuzzyEquals(fixupDirection(direction), fixupDirection(other.direction))
        && name == other.name
        && fixupColorName(accentColorName) == fixupColorName(other.accentColorName);
}

} // namespace nx::vms::client::desktop
