#ifndef QN_PTZ_MAPPER_H
#define QN_PTZ_MAPPER_H

#include <QtCore/QMetaType>

#include <utils/math/space_mapper.h>

#include <nx/core/ptz/vector.h>

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
        const QnSpaceMapperPtr<nx::core::ptz::Vector> &inputMapper,
        const QnSpaceMapperPtr<nx::core::ptz::Vector> &outputMapper);

    nx::core::ptz::Vector logicalToDevice(const nx::core::ptz::Vector &position) const
    {
        return m_inputMapper->targetToSource(position);
    }

    nx::core::ptz::Vector deviceToLogical(const nx::core::ptz::Vector& position) const
    {
        return m_outputMapper->sourceToTarget(position);
    }

    const QnPtzLimits& logicalLimits() const
    {
        return m_logicalLimits;
    }

private:
    QnSpaceMapperPtr<nx::core::ptz::Vector> m_inputMapper;
    QnSpaceMapperPtr<nx::core::ptz::Vector> m_outputMapper;
    QnPtzLimits m_logicalLimits;
};

Q_DECLARE_METATYPE(QnPtzMapperPtr)

#endif // QN_PTZ_MAPPER_H
