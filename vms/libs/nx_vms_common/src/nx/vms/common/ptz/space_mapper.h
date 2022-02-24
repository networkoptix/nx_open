// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

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
    std::map<QString, QnSpaceMapperPtr<qreal>> m_mappers;
};

} // namespace ptz
} // namespace nx::vms::common
