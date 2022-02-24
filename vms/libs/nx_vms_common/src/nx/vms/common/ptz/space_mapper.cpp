// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "space_mapper.h"

namespace nx::vms::common {
namespace ptz {

namespace {

static const QString kPan = "Pan";
static const QString kTilt = "Tilt";
static const QString kRotation = "Rotation";
static const QString kZoom = "Zoom";

} // namespace

SpaceMapper::SpaceMapper(
    const QnSpaceMapperPtr<qreal>& panMapper,
    const QnSpaceMapperPtr<qreal>& tiltMapper,
    const QnSpaceMapperPtr<qreal>& rotationMapper,
    const QnSpaceMapperPtr<qreal>& zoomMapper)
    :
    m_mappers({
        {kPan, panMapper},
        {kTilt, tiltMapper},
        {kRotation, rotationMapper},
        {kZoom, zoomMapper}
    })
{
}

Vector SpaceMapper::sourceToTarget(const Vector& source) const
{
    return Vector(
        m_mappers.at(kPan)->sourceToTarget(source.pan),
        m_mappers.at(kTilt)->sourceToTarget(source.tilt),
        m_mappers.at(kRotation)->sourceToTarget(source.rotation),
        m_mappers.at(kZoom)->sourceToTarget(source.zoom));
}

Vector SpaceMapper::targetToSource(const Vector& target) const
{
    return Vector(
        m_mappers.at(kPan)->targetToSource(target.pan),
        m_mappers.at(kTilt)->targetToSource(target.tilt),
        m_mappers.at(kRotation)->targetToSource(target.rotation),
        m_mappers.at(kZoom)->targetToSource(target.zoom));
}

} // namespace ptz
} // namespace nx::vms::common
