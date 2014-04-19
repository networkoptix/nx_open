#ifndef QN_SERIALIZATION_DEBUG_FWD_H
#define QN_SERIALIZATION_DEBUG_FWD_H

#include <utils/fusion/fusion_fwd.h>

class QDebug;

#define QN_FUSION_DECLARE_FUNCTIONS_debug(TYPE, ... /* PREFIX */)               \
__VA_ARGS__ QDebug &operator<<(QDebug &stream, const TYPE &value);

#endif // QN_SERIALIZATION_DEBUG_FWD_H
