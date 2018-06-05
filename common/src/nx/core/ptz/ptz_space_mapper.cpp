#include "ptz_space_mapper.h"

namespace nx {
namespace core {
namespace ptz {

namespace {

static const QString kPan = lit("Pan");
static const QString kTilt = lit("Tilt");
static const QString kRotation = lit("Rotation");
static const QString kZoom = lit("Zoom");

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

PtzVector SpaceMapper::sourceToTarget(const PtzVector& source) const
{
    return PtzVector(
        m_mappers.at(kPan)->sourceToTarget(source.pan),
        m_mappers.at(kTilt)->sourceToTarget(source.tilt),
        m_mappers.at(kRotation)->sourceToTarget(source.rotation),
        m_mappers.at(kZoom)->sourceToTarget(source.zoom));
}

PtzVector SpaceMapper::targetToSource(const PtzVector& target) const
{
    return PtzVector(
        m_mappers.at(kPan)->targetToSource(target.pan),
        m_mappers.at(kTilt)->targetToSource(target.tilt),
        m_mappers.at(kRotation)->targetToSource(target.rotation),
        m_mappers.at(kZoom)->targetToSource(target.zoom));
}

} // namespace ptz
} // namespace core
} // namespace nx
