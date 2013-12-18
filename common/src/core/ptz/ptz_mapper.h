#ifndef QN_PTZ_MAPPER_H
#define QN_PTZ_MAPPER_H

#include <QtCore/QMetaType>

#include <utils/math/space_mapper.h>

#include "ptz_fwd.h"
#include "ptz_limits.h"

class QnPtzMapperPrivate;

class QnPtzMapper {
public:
    /**
     * Note that in both mappers source is device coordinates and target is logical coordinates.
     *
     * \param inputMapper               Mapper to use when sending data to the camera.
     * \param outputMapper              Mapper to use when receiving data from the camera.
     */
    QnPtzMapper(const QnSpaceMapperPtr<QVector3D> &inputMapper, const QnSpaceMapperPtr<QVector3D> &outputMapper);

    QVector3D logicalToDevice(const QVector3D &position) const {
        return m_inputMapper->targetToSource(position);
    }

    QVector3D deviceToLogical(const QVector3D &position) const {
        return m_outputMapper->sourceToTarget(position);
    }

    const QnPtzLimits &logicalLimits() const {
        return m_logicalLimits;
    }

private:
    QnSpaceMapperPtr<QVector3D> m_inputMapper, m_outputMapper;
    QnPtzLimits m_logicalLimits;
};

Q_DECLARE_METATYPE(QnPtzMapperPtr)

#endif // QN_PTZ_MAPPER_H
