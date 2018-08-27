#pragma once

#include <map>

#include <nx/core/ptz/vector.h>
#include <utils/math/space_mapper.h>

namespace nx {
namespace core {
namespace ptz {

class SpaceMapper: public QnSpaceMapper<Vector>
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
} // namespace core
} // namespace nx
