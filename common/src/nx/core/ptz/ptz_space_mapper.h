#pragma once

#include <map>

#include <nx/core/ptz/ptz_vector.h>
#include <utils/math/space_mapper.h>

namespace nx {
namespace core {
namespace ptz {

class SpaceMapper: public QnSpaceMapper<PtzVector>
{
public:
    SpaceMapper(
        const QnSpaceMapperPtr<qreal>& panMapper,
        const QnSpaceMapperPtr<qreal>& tiltMapper,
        const QnSpaceMapperPtr<qreal>& rotationMapper,
        const QnSpaceMapperPtr<qreal>& zoomMapper);

    virtual PtzVector sourceToTarget(const PtzVector& source) const override;

    virtual PtzVector targetToSource(const PtzVector& target) const override;

private:
    std::map<QString, QnSpaceMapperPtr<qreal>> m_mappers;
};

} // namespace ptz
} // namespace core
} // namespace nx
