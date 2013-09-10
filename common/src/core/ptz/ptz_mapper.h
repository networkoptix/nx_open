#ifndef QN_PTZ_MAPPER_H
#define QN_PTZ_MAPPER_H

#include "ptz_fwd.h"

#include <utils/math/space_mapper.h>

bool deserialize(const QVariant &value, QnPtzMapperPtr *target);

#endif // QN_PTZ_MAPPER_H
