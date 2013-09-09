#ifndef QN_PTZ_MAPPER_H
#define QN_PTZ_MAPPER_H

#include <utils/math/space_mapper.h>

typedef QnSpaceMapperPtr<QVector3D> QnPtzMapperPtr;

bool deserialize(const QVariant &value, QnPtzMapperPtr *target)

#endif // QN_PTZ_MAPPER_H
