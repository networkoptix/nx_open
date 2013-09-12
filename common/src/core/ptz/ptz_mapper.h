#ifndef QN_PTZ_MAPPER_H
#define QN_PTZ_MAPPER_H

#include "ptz_fwd.h"

#include <utils/math/space_mapper.h>

class QnPtzMapperPrivate;

class QnPtzMapper {
public:
    QnPtzMapper(const QnSpaceMapperPtr<QVector3D> &logicalToDevice, const QnSpaceMapperPtr<QVector3D> &deviceToLogical);

    QVector3D logicalToDevice(const QVector3D &position) const {
        return m_logicalToDevice->targetToSource(position);
    }

    QVector3D deviceToLogical(const QVector3D &position) const {
        return m_deviceToLogical->sourceToTarget(position);
    }

    const QnPtzLimits &logicalLimits() const {
        return m_logicalLimits;
    }

private:
    QnSpaceMapperPtr<QVector3D> m_logicalToDevice, m_deviceToLogical;
    QnPtzLimits m_logicalLimits;
};

bool deserialize(const QVariant &value, QnPtzMapperPtr *target);

#endif // QN_PTZ_MAPPER_H
