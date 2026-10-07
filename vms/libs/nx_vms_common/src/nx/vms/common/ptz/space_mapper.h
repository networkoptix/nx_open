// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>
#include <utility>

#include <utils/math/space_mapper.h>

#include "vector.h"

namespace nx::vms::common {
namespace ptz {

class NX_VMS_COMMON_API SpaceMapper: public QnSpaceMapper<Vector>
{
public:
    SpaceMapper(
        const QnSpaceMapperPtr<qreal>& panMapper,
        const QnSpaceMapperPtr<qreal>& tiltMapper,
        const QnSpaceMapperPtr<qreal>& rotationMapper,
        const QnSpaceMapperPtr<qreal>& zoomMapper);

    virtual Vector sourceToTarget(const Vector& source) const override;

    virtual Vector targetToSource(const Vector& target) const override;

private:
    enum class Axis
    {
        pan,
        tilt,
        rotation,
        zoom,

        /** Number of axes, used as the size of m_mappers. Not an axis; must stay last. */
        count
    };

    const QnSpaceMapperPtr<qreal>& mapper(Axis axis) const
    {
        return m_mappers[std::to_underlying(axis)];
    }

private:
    std::array<QnSpaceMapperPtr<qreal>, std::to_underlying(Axis::count)> m_mappers;
};

} // namespace ptz
} // namespace nx::vms::common
