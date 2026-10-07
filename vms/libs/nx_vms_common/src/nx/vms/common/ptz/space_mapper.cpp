// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "space_mapper.h"

namespace nx::vms::common {
namespace ptz {

SpaceMapper::SpaceMapper(const QnSpaceMapperPtr<qreal>& panMapper,
    const QnSpaceMapperPtr<qreal>& tiltMapper,
    const QnSpaceMapperPtr<qreal>& rotationMapper,
    const QnSpaceMapperPtr<qreal>& zoomMapper):
    m_mappers({panMapper, tiltMapper, rotationMapper, zoomMapper})
{
}

Vector SpaceMapper::sourceToTarget(const Vector& source) const
{
    return Vector(mapper(Axis::pan)->sourceToTarget(source.pan),
        mapper(Axis::tilt)->sourceToTarget(source.tilt),
        mapper(Axis::rotation)->sourceToTarget(source.rotation),
        mapper(Axis::zoom)->sourceToTarget(source.zoom));
}

Vector SpaceMapper::targetToSource(const Vector& target) const
{
    return Vector(mapper(Axis::pan)->targetToSource(target.pan),
        mapper(Axis::tilt)->targetToSource(target.tilt),
        mapper(Axis::rotation)->targetToSource(target.rotation),
        mapper(Axis::zoom)->targetToSource(target.zoom));
}

} // namespace ptz
} // namespace nx::vms::common
