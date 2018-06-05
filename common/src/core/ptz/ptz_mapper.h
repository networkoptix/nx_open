#ifndef QN_PTZ_MAPPER_H
#define QN_PTZ_MAPPER_H

#include <QtCore/QMetaType>

#include <utils/math/space_mapper.h>

#include "ptz_fwd.h"
#include "ptz_limits.h"

class QnPtzMapperPrivate;

class QnPtzMapper
{
public:
    /**
     * Note that in both mappers source is device coordinates and target is logical coordinates.
     *
     * \param inputMapper               Mapper to use when sending data to the camera.
     * \param outputMapper              Mapper to use when receiving data from the camera.
     */
    QnPtzMapper(
        const QnSpaceMapperPtr<nx::core::ptz::PtzVector> &inputMapper,
        const QnSpaceMapperPtr<nx::core::ptz::PtzVector> &outputMapper);

    nx::core::ptz::PtzVector logicalToDevice(const nx::core::ptz::PtzVector &position) const
    {
        return m_inputMapper->targetToSource(position);
    }

    nx::core::ptz::PtzVector deviceToLogical(const nx::core::ptz::PtzVector& position) const
    {
        return m_outputMapper->sourceToTarget(position);
    }

    const QnPtzLimits& logicalLimits() const
    {
        return m_logicalLimits;
    }

private:
    QnSpaceMapperPtr<nx::core::ptz::PtzVector> m_inputMapper;
    QnSpaceMapperPtr<nx::core::ptz::PtzVector> m_outputMapper;
    QnPtzLimits m_logicalLimits;
};

Q_DECLARE_METATYPE(QnPtzMapperPtr)

#endif // QN_PTZ_MAPPER_H
